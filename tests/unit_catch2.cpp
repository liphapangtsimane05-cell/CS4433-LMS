#include <catch2/catch_test_macros.hpp>
#include "model.hpp"
#include "security.hpp"
#include "service.hpp"
#include <stdexcept>
#include <cmath>
using namespace lms;

TEST_CASE("Equivalence classes and boundary values for daily penalties", "[unit][penalty]") {
 DailyTenPercentPenalty p;
 SECTION("On time and boundaries") {
  REQUIRE(p.calculate(80,100,0)==80);
  REQUIRE(p.calculate(80,100,1)==70);
  REQUIRE(p.calculate(80,100,2)==60);
  REQUIRE(p.calculate(80,100,3)==50);
  REQUIRE(p.calculate(80,100,4)==0);
 }
 SECTION("Maximum and floor") {
  REQUIRE(p.calculate(5,100,1)==0);
  REQUIRE(p.calculate(100,100,3)==70);
  REQUIRE(p.calculate(0,100,0)==0);
 }
 SECTION("Invalid partitions") {
  REQUIRE_THROWS_AS(p.calculate(-1,100,0),std::invalid_argument);
  REQUIRE_THROWS_AS(p.calculate(101,100,0),std::invalid_argument);
  REQUIRE_THROWS_AS(p.calculate(10,0,0),std::invalid_argument);
  REQUIRE_THROWS_AS(p.calculate(10,100,-1),std::invalid_argument);
 }
}
TEST_CASE("UTC deadline validation includes leap and end-of-day boundaries", "[unit][deadline]") {
 REQUIRE(isoUtc("2028-02-29T23:59:59Z")=="2028-02-29T23:59:59Z");
 REQUIRE_THROWS_AS(isoUtc("2027-02-29T00:00:00Z"),std::invalid_argument);
 REQUIRE_THROWS_AS(isoUtc("2026-11-01T24:00:00Z"),std::invalid_argument);
 REQUIRE_THROWS_AS(isoUtc("2026-11-01T23:60:00Z"),std::invalid_argument);
 REQUIRE_THROWS_AS(isoUtc("2026-04-31T12:00:00Z"),std::invalid_argument);
 REQUIRE_THROWS_AS(isoUtc("2026-11-01 12:00:00"),std::invalid_argument);
}
TEST_CASE("Role inheritance enforces allowed actions", "[unit][rbac]") {
 auto admin=makeUser(1,"a",Role::ADMIN);
 auto lecturer=makeUser(2,"l",Role::LECTURER);
 auto hod=makeUser(3,"h",Role::HOD);
 auto student=makeUser(4,"s",Role::STUDENT);
 REQUIRE(admin->can("admin")); REQUIRE_FALSE(admin->can("study"));
 REQUIRE(lecturer->can("teach")); REQUIRE_FALSE(lecturer->can("moderate"));
 REQUIRE(hod->can("moderate")); REQUIRE_FALSE(hod->can("admin"));
 REQUIRE(student->can("study")); REQUIRE_FALSE(student->can("teach"));
 REQUIRE(parseRole("ADMIN")==Role::ADMIN);
 REQUIRE(parseRole("STUDENT")==Role::STUDENT);
 REQUIRE_THROWS_AS(parseRole("GUEST"),std::invalid_argument);
}
TEST_CASE("Password hashing is salted and verifies securely", "[unit][security]") {
 auto first=security::hashPassword("Good-password-123");
 auto second=security::hashPassword("Good-password-123");
 REQUIRE(first!=second);
 REQUIRE(first.find("Good-password-123")==std::string::npos);
 REQUIRE(security::verifyPassword("Good-password-123",first));
 REQUIRE_FALSE(security::verifyPassword("incorrect-password",first));
 REQUIRE_FALSE(security::verifyPassword("Good-password-123","bad-format"));
 REQUIRE_THROWS_AS(security::hashPassword("short"),std::invalid_argument);
}
