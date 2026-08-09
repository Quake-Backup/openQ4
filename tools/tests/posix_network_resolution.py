#!/usr/bin/env python3
"""Regression checks for POSIX network address resolution."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def read(relative_path: str) -> str:
    return (ROOT / relative_path).read_text(encoding="utf-8")


def require(haystack: str, needle: str, context: str) -> None:
    if needle not in haystack:
        raise AssertionError(f"Missing {needle!r} in {context}")


def reject(haystack: str, needle: str, context: str) -> None:
    if needle in haystack:
        raise AssertionError(f"Unexpected {needle!r} in {context}")


def function_body(source: str, signature: str, context: str) -> str:
    start = source.find(signature)
    if start < 0:
        raise AssertionError(f"Missing {signature!r} in {context}")
    opening = source.find("{", start + len(signature))
    if opening < 0:
        raise AssertionError(f"Missing body for {signature!r} in {context}")
    depth = 0
    for index in range(opening, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[start : index + 1]
    raise AssertionError(f"Unbalanced body for {signature!r} in {context}")


def require_before(haystack: str, first: str, second: str, context: str) -> None:
    require(haystack, first, context)
    require(haystack, second, context)
    if haystack.index(first) >= haystack.index(second):
        raise AssertionError(f"Expected {first!r} before {second!r} in {context}")


def validate_posix_resolver() -> None:
    source = read("src/sys/posix/posix_net.cpp")

    for legacy_symbol in ("gethostbyname", "inet_aton", "StringToSockaddr"):
        reject(source, legacy_symbol, "POSIX network resolver")

    for required_symbol in (
        "getaddrinfo",
        "freeaddrinfo",
        "AI_NUMERICHOST",
        "sockaddr_storage",
        "AF_INET6",
        "NA_IP6",
        "inet_ntop",
        "IPV6_V6ONLY",
        "netSocket6",
    ):
        require(source, required_symbol, "POSIX IPv6-capable network path")

    require(source, "idNetworkEndpoint::Split", "shared endpoint parsing")
    require(source, "SockadrToNetadr", "sockaddr-to-netadr conversion")
    require(source, "NetadrToSockadr", "netadr-to-sockaddr conversion")
    require(source, "recvmsg( socketFd", "shared UDP receive helper")
    require(source, "MSG_TRUNC", "oversize UDP datagram detection")
    require(source, "sendto( socketFd", "family-selected UDP send")
    require(source, "select( maxSocket + 1", "dual-socket blocking receive")
    reject(source, "struct sockaddr_in sadr", "TCP resolver")
    reject(source, "struct sockaddr_in from", "UDP receive path")
    reject(source, "struct sockaddr_in addr", "UDP send path")

    require(
        source,
        'idCVar net_port( "net_port", "0", CVAR_SYSTEM | CVAR_INTEGER, "local IP port number" );',
        "POSIX automatic net_port default",
    )
    reject(source, 'idCVar net_port( "net_port", "",', "POSIX empty net_port default")

    socket_helper = function_body(source, "static int IPSocketForFamily", "POSIX UDP family bind")
    require(socket_helper, "bool *addressResolved = NULL", "POSIX bind-resolution result")
    require(socket_helper, "*addressResolved = false;", "POSIX bind-resolution initialization")
    require(socket_helper, "*addressResolved = true;", "POSIX successful address resolution")

    port_init = function_body(source, "bool idPort::InitForPort", "POSIX UDP initialization")
    any_interface = function_body(
        port_init,
        "if ( Sys_IsAnyInterfaceName( interfaceName ) )",
        "POSIX default dual-stack bind",
    )
    require_before(
        any_interface,
        "IPSocketForFamily( interfaceName, portNumber, AF_INET, &bound4, true )",
        "if ( netSocket <= 0 )",
        "POSIX required default IPv4 bind",
    )
    require_before(
        any_interface,
        "if ( netSocket <= 0 )",
        "IPSocketForFamily( interfaceName, ipv6Port, AF_INET6, &bound6, true )",
        "POSIX optional default IPv6 bind",
    )
    ipv4_failure = function_body(any_interface, "if ( netSocket <= 0 )", "POSIX failed default IPv4 bind")
    require(ipv4_failure, "return false;", "POSIX failed default IPv4 bind")
    reject(any_interface, "bound_to = bound6;", "POSIX IPv6-only default bind")

    for token in (
        "const int firstFamily = prefersIPv6 ? AF_INET6 : AF_INET;",
        "bool firstFamilyResolved = false;",
        "&explicitBound, true, &firstFamilyResolved",
        "if ( isAddressLiteral || firstFamilyResolved )",
    ):
        require(port_init, token, "POSIX explicit hostname bind")
    preferred_failure = function_body(
        port_init,
        "if ( isAddressLiteral || firstFamilyResolved )",
        "POSIX resolved preferred-family bind failure",
    )
    require(preferred_failure, "return false;", "POSIX resolved preferred-family bind failure")
    reject(preferred_failure, "secondSocket", "POSIX resolved preferred-family bind failure")
    require_before(
        port_init,
        "if ( isAddressLiteral || firstFamilyResolved )",
        "IPSocketForFamily( interfaceName, portNumber, secondFamily",
        "POSIX cross-family hostname fallback",
    )


def validate_public_netadr_shape() -> None:
    source = read("src/sys/sys_public.h")

    for required_symbol in ("NA_IP6", "ip6[16]", "scopeId", "netSocket6"):
        require(source, required_symbol, "public net address shape")


def validate_legacy_message_format() -> None:
    source = read("src/idlib/BitMsg.cpp")

    require(source, "adr.type == NA_IP || adr.type == NA_LOOPBACK", "legacy netadr write filter")
    require(source, "memset( adr, 0, sizeof( *adr ) );", "legacy netadr read initialization")
    require(source, "adr->type = NA_IP;", "legacy netadr read type")


def validate_release_note() -> None:
    source = read("docs/dev/release-completion.md")

    require(source, "POSIX networking now uses modern address resolution", "release completion notes")
    require(source, "IPv6 literals and AAAA records", "release completion notes")


def main() -> None:
    validate_posix_resolver()
    validate_public_netadr_shape()
    validate_legacy_message_format()
    validate_release_note()
    print("posix_network_resolution: ok")


if __name__ == "__main__":
    main()
