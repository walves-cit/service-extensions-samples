// Copyright 2025 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <grpcpp/grpcpp.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/server_context.h>

#include <fstream>

#include "absl/log/log.h"
#include "envoy/service/ext_proc/v3/external_processor.grpc.pb.h"
#include "envoy/service/ext_proc/v3/external_processor.pb.h"

using envoy::config::core::v3::HeaderValue;
using envoy::config::core::v3::HeaderValueOption;
using envoy::service::ext_proc::v3::BodyMutation;
using envoy::service::ext_proc::v3::ExternalProcessor;
using envoy::service::ext_proc::v3::HeaderMutation;
using envoy::service::ext_proc::v3::ProcessingRequest;
using envoy::service::ext_proc::v3::ProcessingResponse;

class CalloutServer : public ExternalProcessor::Service {
 public:
  // Adds a request header field.
  static void AddRequestHeader(ProcessingResponse* response,
                               std::string_view key, std::string_view value) {
    HeaderValue* new_header = response->mutable_request_headers()
                                  ->mutable_response()
                                  ->mutable_header_mutation()
                                  ->add_set_headers()
                                  ->mutable_header();
    new_header->set_key(key);
    new_header->set_value(value);
  }

  // Replaces a request header field.
  static void ReplaceRequestHeader(ProcessingResponse* response,
                                   std::string_view key,
                                   std::string_view value) {
    HeaderValueOption* new_header_option = response->mutable_request_headers()
                                               ->mutable_response()
                                               ->mutable_header_mutation()
                                               ->add_set_headers();
    new_header_option->set_append_action(
        HeaderValueOption::OVERWRITE_IF_EXISTS_OR_ADD);
    HeaderValue* new_header = new_header_option->mutable_header();
    new_header->set_key(key);
    new_header->set_value(value);
  }

  // Adds a response header field.
  static void AddResponseHeader(ProcessingResponse* response,
                                std::string_view key, std::string_view value) {
    HeaderValue* new_header = response->mutable_response_headers()
                                  ->mutable_response()
                                  ->mutable_header_mutation()
                                  ->add_set_headers()
                                  ->mutable_header();
    new_header->set_key(key);
    new_header->set_value(value);
  }

  // Replaces a response header field.
  static void ReplaceResponseHeader(ProcessingResponse* response,
                                    std::string_view key,
                                    std::string_view value) {
    HeaderValueOption* new_header_option = response->mutable_response_headers()
                                               ->mutable_response()
                                               ->mutable_header_mutation()
                                               ->add_set_headers();
    new_header_option->set_append_action(
        HeaderValueOption::OVERWRITE_IF_EXISTS_OR_ADD);
    HeaderValue* new_header = new_header_option->mutable_header();
    new_header->set_key(key);
    new_header->set_value(value);
  }

  // Removes a response header field.
  static void RemoveResponseHeader(ProcessingResponse* response,
                                   std::string_view header_name) {
    auto* headers_mutation = response->mutable_response_headers()
                                 ->mutable_response()
                                 ->mutable_header_mutation();
    headers_mutation->add_remove_headers(header_name);
  }

  // Replaces a request body field.
  static void ReplaceRequestBody(ProcessingResponse* response,
                                 std::string_view body) {
    response->mutable_request_body()
        ->mutable_response()
        ->mutable_body_mutation()
        ->set_body(body);
  }

  // Replaces a response body field.
  static void ReplaceResponseBody(ProcessingResponse* response,
                                  std::string_view body) {
    response->mutable_response_body()
        ->mutable_response()
        ->mutable_body_mutation()
        ->set_body(body);
  }

  // Creates the SSL secure server credentials given the key and cert path set.
  static std::optional<std::shared_ptr<grpc::ServerCredentials>>
  CreateSecureServerCredentials(std::string_view key_path,
                                std::string_view cert_path) {
    auto key = CalloutServer::ReadDataFile(key_path);
    if (!key.ok()) {
      LOG(ERROR) << "Error reading the private key file on " << key_path;
      return std::nullopt;
    }
    auto cert = CalloutServer::ReadDataFile(cert_path);
    if (!cert.ok()) {
      LOG(ERROR) << "Error reading the certificate file on " << cert_path;
      return std::nullopt;
    }

    grpc::SslServerCredentialsOptions::PemKeyCertPair key_cert_pair;
    key_cert_pair.private_key = *key;
    key_cert_pair.cert_chain = *cert;

    grpc::SslServerCredentialsOptions ssl_options;
    ssl_options.pem_key_cert_pairs.push_back(key_cert_pair);
    return grpc::SslServerCredentials(ssl_options);
  }

  static std::unique_ptr<grpc::Server> RunServer(
      std::string_view server_address, CalloutServer& service) {
    return CalloutServer::RunServer(server_address, service,
                                    grpc::InsecureServerCredentials(), false);
  }

  static std::unique_ptr<grpc::Server> RunServer(
      std::string_view server_address, CalloutServer& service,
      std::shared_ptr<grpc::ServerCredentials> credentials, bool wait) {
    grpc::EnableDefaultHealthCheckService(true);
    grpc::ServerBuilder builder;
    builder.AddListeningPort(std::string{server_address}, credentials);
    builder.RegisterService(&service);

    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    LOG(INFO) << "Envoy Ext Proc server listening on " << server_address;

    if (wait) {
      server->Wait();
    }
    return server;
  }

  grpc::Status Process(
      grpc::ServerContext* context,
      grpc::ServerReaderWriter<ProcessingResponse, ProcessingRequest>* stream)
      override {
    (void)context;

    ProcessingRequest request;
    while (stream->Read(&request)) {
      ProcessingResponse response;
      ProcessRequest(&request, &response);
      stream->Write(response);
    }

    return grpc::Status::OK;
  }

  // Handles request headers.
  virtual void OnRequestHeader(ProcessingRequest* request,
                               ProcessingResponse* response) {
    LOG(INFO) << "OnRequestHeader called.";
  }

  // Handles response headers.
  virtual void OnResponseHeader(ProcessingRequest* request,
                                ProcessingResponse* response) {
    LOG(INFO) << "OnResponseHeader called.";
  }

  // Handles request bodies.
  virtual void OnRequestBody(ProcessingRequest* request,
                             ProcessingResponse* response) {
    LOG(INFO) << "OnRequestBody called.";
  }

  // Handles response bodies.
  virtual void OnResponseBody(ProcessingRequest* request,
                              ProcessingResponse* response) {
    LOG(INFO) << "OnResponseBody called.";
  }

 private:
  static absl::StatusOr<std::string> ReadDataFile(std::string_view path) {
    std::ifstream file(std::string{path}, std::ios::binary);
    if (file.fail()) {
      return absl::NotFoundError(
          absl::StrCat("failed to open: ", path, ", error: ", strerror(errno)));
    }
    std::stringstream file_string_stream;
    file_string_stream << file.rdbuf();
    return file_string_stream.str();
  }

  void ProcessRequest(ProcessingRequest* request,
                      ProcessingResponse* response) {
    switch (request->request_case()) {
      case ProcessingRequest::RequestCase::kRequestHeaders:
        this->OnRequestHeader(request, response);
        break;
      case ProcessingRequest::RequestCase::kResponseHeaders:
        this->OnResponseHeader(request, response);
        break;
      case ProcessingRequest::RequestCase::kRequestBody:
        this->OnRequestBody(request, response);
        break;
      case ProcessingRequest::RequestCase::kResponseBody:
        this->OnResponseBody(request, response);
        break;
      case ProcessingRequest::RequestCase::kRequestTrailers:
      case ProcessingRequest::RequestCase::kResponseTrailers:
        break;
      case ProcessingRequest::RequestCase::REQUEST_NOT_SET:
      default:
        LOG(WARNING) << "Received a ProcessingRequest with no request data.";
        break;
    }
  }
};
