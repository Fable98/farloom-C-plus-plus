#include "test_framework.hpp"
#include "router.hpp"

using namespace farloom;
using namespace farloom::http;

Request make_req(Method method, std::string path) {
    Request r;
    r.method = std::move(method);
    r.path = std::move(path);
    return r;
}

void test_router_functionality() {
    Router r;
    r.get("/a", [](const Request&) { return Response::new_resp(StatusCode::Ok); });
    r.post("/a", [](const Request&) { return Response::new_resp(StatusCode::NotFound); });
    r.post("/p", [](const Request&) { return Response::new_resp(StatusCode::Ok); });

    auto m1 = r.lookup(Method(MethodKind::Get), "/a");
    ASSERT_EQ(m1.kind, RouteMatchKind::Found);
    ASSERT_EQ(status_code((*m1.handler)(make_req(Method(MethodKind::Get), "/a")).status), 200);

    auto m2 = r.lookup(Method(MethodKind::Post), "/a");
    ASSERT_EQ(m2.kind, RouteMatchKind::Found);
    ASSERT_EQ(status_code((*m2.handler)(make_req(Method(MethodKind::Post), "/a")).status), 404);

    auto m3 = r.lookup(Method(MethodKind::Get), "/p");
    ASSERT_EQ(m3.kind, RouteMatchKind::MethodNotAllowed);
    std::vector<Method> expected_allow = {Method(MethodKind::Post)};
    ASSERT_EQ(m3.allow, expected_allow);

    auto m4 = r.lookup(Method(MethodKind::Other, "DELETE"), "/a");
    ASSERT_EQ(m4.kind, RouteMatchKind::MethodNotAllowed);
    std::vector<Method> expected_allow_a = {Method(MethodKind::Get), Method(MethodKind::Post)};
    ASSERT_EQ(m4.allow, expected_allow_a);

    ASSERT_EQ(r.lookup(Method(MethodKind::Get), "/missing").kind, RouteMatchKind::NotFound);
    ASSERT_EQ(r.lookup(Method(MethodKind::Get), "/a/").kind, RouteMatchKind::NotFound);
    ASSERT_EQ(r.lookup(Method(MethodKind::Get), "/%61").kind, RouteMatchKind::NotFound);
}

void test_builtins() {
    auto r = Router::with_builtins();
    auto m_health = r.lookup(Method(MethodKind::Get), "/health");
    ASSERT_EQ(m_health.kind, RouteMatchKind::Found);
    auto resp_health = (*m_health.handler)(make_req(Method(MethodKind::Get), "/health"));
    ASSERT_EQ(status_code(resp_health.status), 200);
    std::string hbody(resp_health.body.begin(), resp_health.body.end());
    ASSERT_EQ(hbody, R"({"status":"ok"})");

    auto m_echo = r.lookup(Method(MethodKind::Post), "/api/echo");
    ASSERT_EQ(m_echo.kind, RouteMatchKind::Found);
    Request echo_req = make_req(Method(MethodKind::Post), "/api/echo");
    std::string edata = "hello echo";
    echo_req.body = std::vector<uint8_t>(edata.begin(), edata.end());
    echo_req.headers.insert("Content-Type", "text/plain");
    auto resp_echo = (*m_echo.handler)(echo_req);
    ASSERT_EQ(status_code(resp_echo.status), 200);
    std::string ebody(resp_echo.body.begin(), resp_echo.body.end());
    ASSERT_EQ(ebody, "hello echo");

    auto m_echo_get = r.lookup(Method(MethodKind::Get), "/api/echo");
    ASSERT_EQ(m_echo_get.kind, RouteMatchKind::MethodNotAllowed);
    std::vector<Method> allow_echo = {Method(MethodKind::Post)};
    ASSERT_EQ(m_echo_get.allow, allow_echo);
}

int main() {
    RUN_TEST(test_router_functionality);
    RUN_TEST(test_builtins);
    return 0;
}
