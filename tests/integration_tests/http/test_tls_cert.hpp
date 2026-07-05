#pragma once

#include <filesystem>
#include <fstream>
#include <ios>
#include <string>
#include <string_view>

#include <unistd.h>  // getpid — per-process-unique temp paths (parallel ctest safe)

// NOT A SECRET: a throwaway self-signed cert + key generated solely for these
// localhost integration tests (CN=localhost, SAN IP:127.0.0.1, valid to 2126).
// It guards no real asset — present so the TLS tests are hermetic. If a CI secret
// scanner flags this file, allow-list it.
namespace http_tls_test {

    inline constexpr std::string_view kTestCertPem = R"PEM(
-----BEGIN CERTIFICATE-----
MIIDJzCCAg+gAwIBAgIUanuJILFxik0+52ml2z09lgvcxt8wDQYJKoZIhvcNAQEL
BQAwFDESMBAGA1UEAwwJbG9jYWxob3N0MCAXDTI2MDYxNTE3NTAxNVoYDzIxMjYw
NTIyMTc1MDE1WjAUMRIwEAYDVQQDDAlsb2NhbGhvc3QwggEiMA0GCSqGSIb3DQEB
AQUAA4IBDwAwggEKAoIBAQDASeExUpCEClO0XKr8FOZ9uT4s0P/KOp6g8T4WmL8M
DAB5y32B7r67bDP1Vvs/1Ryh0PVjmke8575a7pKclrbxOzoGz7hI057PhhMzexwn
KvhN5zm63ddSq+whkcOIfLoklYLKhGdAht8eLJI17J64o+KmYybT2Ln2YsZLj9bU
3+3YS5M1sgjohOJT2mb87w5C95jsxPgurMwLWxybujgRxTnl9hhhR0rWJr6ZuTYq
zPxDT5n8MR48P6BiY4j5ncIUSSR+jGzvDLztWBwKyKLOL4rAgyLowciR930t79L1
nN992WUWrhhW39a0Xmy8P0xGaHt3qvaEvqg8AY7tXqXfAgMBAAGjbzBtMB0GA1Ud
DgQWBBSXP9hvZ3ZWKO9N9jk19ImQ6VhvMjAfBgNVHSMEGDAWgBSXP9hvZ3ZWKO9N
9jk19ImQ6VhvMjAPBgNVHRMBAf8EBTADAQH/MBoGA1UdEQQTMBGHBH8AAAGCCWxv
Y2FsaG9zdDANBgkqhkiG9w0BAQsFAAOCAQEAMT2RYqCJtA2WLgsTM0LgJPo1i1Pn
B1P1V5hThhvkgaeRJHCB7Hnmv2MzdQUvLJTx/dDqvZkZQ1ZkIg7bVt9yzD+sN/9P
NLsJ5nHJl6LbmowztKwS2JPs3pdMYMjzPddq0rrOaZoqPzWNdAPjZrTwDsSOu4Ph
Db7eEXEjGyVcKlB0IV5fy5Oq5+qBezmiE1zjmidn+U3NFqacGauyGTkVfBasxwOe
9mFkshyoJ++T7crREDl6psvSZHpt2thk5EVC2nQfGWnrZUwxRrumFllmL6HgiIeR
ck99JTvJ+HqPD5IrcEV/UI8kkexGJWQAo1R+TXYVKsmgIHGwKYm1tLD0MA==
-----END CERTIFICATE-----
)PEM";

    inline constexpr std::string_view kTestKeyPem = R"PEM(
-----BEGIN PRIVATE KEY-----
MIIEvAIBADANBgkqhkiG9w0BAQEFAASCBKYwggSiAgEAAoIBAQDASeExUpCEClO0
XKr8FOZ9uT4s0P/KOp6g8T4WmL8MDAB5y32B7r67bDP1Vvs/1Ryh0PVjmke8575a
7pKclrbxOzoGz7hI057PhhMzexwnKvhN5zm63ddSq+whkcOIfLoklYLKhGdAht8e
LJI17J64o+KmYybT2Ln2YsZLj9bU3+3YS5M1sgjohOJT2mb87w5C95jsxPgurMwL
WxybujgRxTnl9hhhR0rWJr6ZuTYqzPxDT5n8MR48P6BiY4j5ncIUSSR+jGzvDLzt
WBwKyKLOL4rAgyLowciR930t79L1nN992WUWrhhW39a0Xmy8P0xGaHt3qvaEvqg8
AY7tXqXfAgMBAAECggEAONdeD0N13uJingVqsfvHqtCQlZTumCw96huGHA3pI7mE
hnxlzHvzu9mffl3JBbSMszTe5SOdIzVqKt0tT8apq6OzYoIS2sxbvMLIeEZjKxzj
q7u3cArV9OVHdyDsqTMdn2Tm9dCv6P41hGjui6w3uyMPA9p5htQhHLlUHtAVVHWd
rhxcMr6BAyUxERVL8EXHxiGOwHqanwKWRzcngmqXLv8QOGWt/rtFvmg1MoP/FOSI
nraQOnoFWLmj//dhZKD/Dw2eaHkjtqPMqaXS93CKIHZmoQfaTKgLIxi7erIJrZfx
DUqj5jrbfr7cNsuVfpouupFi2OG9rEhuBHTqzJyASQKBgQD17hJRFgYQoxa6jbfr
VOnrpMUUlWIy05WK+2/CjfbRE7xRPmyBYmUX4xMdTXSPva6RaseBlO4gz4pRXZDQ
GLtCzQAC0bap8uP2m/uWw6l5NHwhtxKzXTdwifViBgyfZ2oZyxihbjMi3K6UUImn
Gaha2zOg0ybuWhmrWU8y53G7tQKBgQDIKYT8NKWQaWWW9+h1ek1qqc7sLbdu8+yf
AUkSLH+UKgWW01t3g4m/6Qw1eSDHWC3l0IV3adXNg1gtmutBnpwsy1pzCL8H2PMe
6d/BsKGG6APKnlgrguTMWsi8PstRejkvlMK7Dq7DMGY1ncdDYS7qfA2aKqwkoTpb
oiH2upLfwwKBgEf/BFm8qtXgCN1gc8FvQHP97rxR50ed7Z+ccGFykhkvP+hA8B8I
oTPXBFeFv2P9Uce8jN+ArB3q5EFhtO1W8CtkPGaW4nTqaJZfn83JRin3lYeBQvZD
ieFmYfHqd3OLIOKgNHu9+TZxiKJe2Y2T01eV6I1ig3kv42foY2kxnHgpAoGAX4za
a97h7jcyBMhhUrtIe5OGMN5+A1wz54+ghylw2ZTZyC8rKblEJ7WjW19wU1j3yA4r
uF5wbsO1c0fR6ChEG2oTyngxYRiirm4sn3SnFxRowu+l3VeFyzvHOX2sZz+2Ts1v
zAXtTUYsdInWFocs80i24ZJfTLked6HFHtffxysCgYBea5HspclFpe2E9C/GUo8V
y/B99AQhfv0FuxWhhkcilwdtDNmvYMhWKowxE38ifITJzbjvggQTzLOOgM8cprZF
4EnmgaePmDrECghmrZQ8Rqm4q8/hDEndsGG5/ulVkQ8tgcMM+jvmCgkksh7n7rwy
Qt2yOuRaaXafFa8Zs9HObA==
-----END PRIVATE KEY-----
)PEM";

    /// Write `contents` to a per-process-unique temp file; return its path.
    inline std::string write_temp(std::string_view stem, std::string_view contents) {
        const std::filesystem::path path = std::filesystem::temp_directory_path() /
                                           ("dmp_http_" + std::to_string(::getpid()) + "_" + std::string{stem});
        std::ofstream out{path, std::ios::binary | std::ios::trunc};
        // TODO: check write success (out.exceptions(std::ios::failbit) or out.good()) so a failed temp write reports
        // clearly instead of as an opaque OpenSSL parse error.
        out.write(contents.data(), static_cast<std::streamsize>(contents.size()));
        return path.string();
    }

}  // namespace http_tls_test
