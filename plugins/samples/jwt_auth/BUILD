load("//:plugins.bzl", "proxy_wasm_plugin_cpp", "proxy_wasm_plugin_rust", "proxy_wasm_tests")

licenses(["notice"])  # Apache 2

proxy_wasm_plugin_cpp(
    name = "plugin_cpp.wasm",
    srcs = ["plugin.cc"],
    deps = [
        "@proxy_wasm_cpp_sdk//contrib:contrib_lib",
        "@jwt_verify_lib//:jwt_verify_lib",
        "//:boost_exception",
        "@boost//:url",
    ],
    linkopts = [
        # To avoid the error:
        # library_pthread.js:26: #error "STANDALONE_WASM does not support shared memories yet".
        # Disabling the pthreads avoids the inclusion of the library_pthread.js.
        "-sUSE_PTHREADS=0",
    ],
)

proxy_wasm_tests(
    name = "tests",
    config = ":publickey.pem",
    plugins = [
        ":plugin_cpp.wasm",
    ],
    tests = ":tests.textpb",
)
