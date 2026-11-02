#!/usr/bin/env python3
"""Controlled end-to-end TLS bump benchmark for the Kontrol-ID test lab.

The tool opens a fresh CONNECT tunnel and TLS session for every request.  It
does not transmit credentials and is intentionally capped unless the operator
explicitly opts into a larger run.
"""

import argparse
import collections
import concurrent.futures
import hashlib
import json
import socket
import ssl
import statistics
import time


def read_headers(sock, limit=65536):
    data = bytearray()
    while b"\r\n\r\n" not in data:
        chunk = sock.recv(4096)
        if not chunk:
            break
        data.extend(chunk)
        if len(data) > limit:
            raise RuntimeError("response headers exceeded the safety limit")
    return bytes(data)


def percentile(values, percentage):
    if not values:
        return 0.0
    ordered = sorted(values)
    position = (len(ordered) - 1) * percentage
    lower = int(position)
    upper = min(lower + 1, len(ordered) - 1)
    fraction = position - lower
    return ordered[lower] * (1.0 - fraction) + ordered[upper] * fraction


def make_context(tls_version):
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
    context.check_hostname = False
    context.verify_mode = ssl.CERT_NONE
    if tls_version == "tls12":
        context.minimum_version = ssl.TLSVersion.TLSv1_2
        context.maximum_version = ssl.TLSVersion.TLSv1_2
    elif tls_version == "tls13":
        context.minimum_version = ssl.TLSVersion.TLSv1_3
        context.maximum_version = ssl.TLSVersion.TLSv1_3
    return context


def one_request(args, context):
    started = time.perf_counter()
    raw = None
    wrapped = None
    try:
        raw = socket.create_connection((args.proxy_host, args.proxy_port), args.timeout)
        raw.settimeout(args.timeout)
        authority = "{}:{}".format(args.target_host, args.target_port)
        request = (
            "CONNECT {0} HTTP/1.1\r\nHost: {0}\r\n"
            "Proxy-Connection: close\r\n\r\n"
        ).format(authority)
        raw.sendall(request.encode("ascii"))
        connect_reply = read_headers(raw)
        first_line = connect_reply.split(b"\r\n", 1)[0]
        if b" 200 " not in first_line:
            raise RuntimeError("CONNECT failed: {}".format(first_line.decode("latin1", "replace")))

        wrapped = context.wrap_socket(raw, server_hostname=args.target_host)
        raw = None
        peer_fingerprint = hashlib.sha256(
            wrapped.getpeercert(binary_form=True)
        ).hexdigest()
        origin_request = (
            "GET {0} HTTP/1.1\r\nHost: {1}\r\n"
            "User-Agent: Kontrol-ID-TLS-Benchmark/1.0\r\n"
            "Connection: close\r\n\r\n"
        ).format(args.path, authority)
        wrapped.sendall(origin_request.encode("ascii"))
        response = bytearray()
        while len(response) <= args.max_response_bytes:
            chunk = wrapped.recv(16384)
            if not chunk:
                break
            response.extend(chunk)
        if len(response) > args.max_response_bytes:
            raise RuntimeError("response exceeded the safety limit")
        status_line = bytes(response).split(b"\r\n", 1)[0]
        parts = status_line.split()
        status = int(parts[1]) if len(parts) > 1 and parts[1].isdigit() else 0
        return time.perf_counter() - started, status, len(response), None, peer_fingerprint
    except Exception as error:  # benchmark errors are aggregated, never hidden
        return time.perf_counter() - started, 0, 0, "{}: {}".format(type(error).__name__, error), None
    finally:
        for candidate in (wrapped, raw):
            if candidate is not None:
                try:
                    candidate.close()
                except OSError:
                    pass


def run_batch(args, context, count, concurrency):
    started = time.perf_counter()
    with concurrent.futures.ThreadPoolExecutor(max_workers=concurrency) as executor:
        results = list(executor.map(lambda _: one_request(args, context), range(count)))
    elapsed = time.perf_counter() - started
    return results, elapsed


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--proxy-host", required=True)
    parser.add_argument("--proxy-port", type=int, default=3128)
    parser.add_argument("--target-host", required=True)
    parser.add_argument("--target-port", type=int, default=9443)
    parser.add_argument("--path", default="/")
    parser.add_argument("--requests", type=int, default=200)
    parser.add_argument("--concurrency", type=int, default=8)
    parser.add_argument("--warmup", type=int, default=10)
    parser.add_argument("--timeout", type=float, default=10.0)
    parser.add_argument("--tls", choices=("default", "tls12", "tls13"), default="tls12")
    parser.add_argument("--max-response-bytes", type=int, default=1048576)
    parser.add_argument("--unsafe-large-run", action="store_true")
    args = parser.parse_args()

    if args.requests < 1 or args.concurrency < 1 or args.warmup < 0:
        parser.error("requests/concurrency must be positive and warmup cannot be negative")
    if not args.unsafe_large_run and (args.requests > 50000 or args.concurrency > 128):
        parser.error("safe limit exceeded; use --unsafe-large-run only in an isolated lab")

    context = make_context(args.tls)
    if args.warmup:
        warmup, _ = run_batch(args, context, args.warmup, min(args.concurrency, args.warmup))
        warmup_errors = [item[3] for item in warmup if item[3]]
        if warmup_errors:
            raise SystemExit("warmup failed: {}".format(warmup_errors[0]))

    results, elapsed = run_batch(args, context, args.requests, args.concurrency)
    latencies = [item[0] * 1000.0 for item in results if item[3] is None]
    statuses = collections.Counter(item[1] for item in results if item[3] is None)
    errors = collections.Counter(item[3] for item in results if item[3] is not None)
    peer_fingerprints = collections.Counter(item[4] for item in results if item[4])
    summary = {
        "requests": args.requests,
        "completed": len(latencies),
        "errors": sum(errors.values()),
        "concurrency": args.concurrency,
        "tls": args.tls,
        "elapsed_seconds": round(elapsed, 6),
        "requests_per_second": round(len(latencies) / elapsed, 3) if elapsed else 0.0,
        "latency_ms": {
            "mean": round(statistics.mean(latencies), 3) if latencies else 0.0,
            "p50": round(percentile(latencies, 0.50), 3),
            "p95": round(percentile(latencies, 0.95), 3),
            "p99": round(percentile(latencies, 0.99), 3),
            "max": round(max(latencies), 3) if latencies else 0.0,
        },
        "http_statuses": dict(sorted(statuses.items())),
        "peer_certificate_sha256": dict(peer_fingerprints),
        "error_types": dict(errors.most_common(10)),
        "bytes_received": sum(item[2] for item in results),
    }
    print(json.dumps(summary, indent=2, sort_keys=True))
    raise SystemExit(0 if not errors else 2)


if __name__ == "__main__":
    main()
