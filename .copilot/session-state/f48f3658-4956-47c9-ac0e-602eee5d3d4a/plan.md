# Phase 12: Standard Library, Networking & Examples

## Goal
Implement stdlib networking (TCP sockets, HTTP), move JSON to stdlib, create
pgl_example/ with a curl client and an annotation-routed web server.

## Approach
1. Add C runtime functions for TCP sockets (socket/bind/listen/accept/connect/send/recv/close)
2. Add function-level annotations (currently only struct fields have them)
3. Add runtime function-annotation reflection API
4. Build stdlib/net/ — low-level socket wrappers in PGL
5. Build stdlib/http/ — HTTP request parsing and response building
6. Move JSON from vendor to stdlib/json/
7. Create pgl_example/curl.pgl — single-file HTTP GET client
8. Create pgl_example/web_server.pgl — annotation-routed web server

## Tasks

### T1: C runtime — TCP socket functions
Add to pangu_builtins.c: tcp_socket, tcp_bind, tcp_listen, tcp_accept,
tcp_connect, tcp_send, tcp_recv, tcp_close, tcp_set_reuseaddr

### T2: Register socket builtins
Register in sema (BUILTIN_FUNCTIONS, param counts, types) and
llvm_backend (getOrInsertFunction declarations)

### T3: Function-level annotations
- Add annotation storage to GFuncDef in grammer/datas.h
- Parse @annotation("value") before func keyword in parser
- Generate function annotation metadata in LLVM backend
- Add runtime reflection: func_annotation_count, func_annotation_key,
  func_annotation_value, func_annotation_func_name, func_count

### T4: stdlib/net/socket.pgl
PGL wrappers for TCP operations: connect, serve, request/response helpers

### T5: stdlib/http/http.pgl
HTTP parsing: parse_request, build_response, status codes, headers

### T6: stdlib/json/json.pgl
Move JSON from vendor to stdlib, clean up and extend

### T7: pgl_example/curl.pgl
Single-file HTTP GET client using stdlib/net + stdlib/http

### T8: pgl_example/web_server.pgl
Annotation-routed web server: @route("GET", "/path") on handler functions,
auto-discovery via reflection at startup
