#!/usr/bin/env bash
set -euo pipefail

IFACE="${CAN_IFACE:-can0}"
NODE_ID="${PUMP_NODE_ID:-0x30}"
CONDUCTOR_ID="${CONDUCTOR_ID:-0x10}"
CAPTURE_SEC="${CAN_CAPTURE_SEC:-0.35}"
BITRATE="${CAN_BITRATE:-1000000}"
SAMPLE_POINT="${CAN_SAMPLE_POINT:-0.750}"

PRIO_NORMAL=1
MSG_COMMAND=0
MSG_ACK=1
MSG_NACK=2
MSG_DATA_DONE_LOG=3

CMD_GET_DEVICE_INFO=0xF001
CMD_PUMP_START=0x0202
CMD_PUMP_STOP=0x0203
CMD_VALVE_OPEN=0x0204
CMD_VALVE_CLOSE=0x0205

usage() {
    cat <<EOF
Usage:
  $0 setup
  $0 info
  $0 pump-start <0..12>
  $0 pump-stop <0..12>
  $0 pump-cycle <0..12> [seconds]
  $0 valve-open <13..15>
  $0 valve-close <13..15>
  $0 send <can_id_hex> <data_hex_8_bytes>

Environment:
  CAN_IFACE=can0
  PUMP_NODE_ID=0x30
  CONDUCTOR_ID=0x10
  CAN_BITRATE=1000000
  CAN_SAMPLE_POINT=0.750
  CAN_CAPTURE_SEC=0.35
EOF
}

hex_id() {
    printf "%08X" "$1"
}

node_dec() {
    printf "%d" "$((NODE_ID))"
}

conductor_dec() {
    printf "%d" "$((CONDUCTOR_ID))"
}

build_id() {
    local priority="$1"
    local msg_type="$2"
    local dst="$3"
    local src="$4"
    hex_id "$(( ((priority & 0x7) << 26) | ((msg_type & 0x3) << 24) | ((dst & 0xFF) << 16) | ((src & 0xFF) << 8) ))"
}

cmd_payload() {
    local cmd="$1"
    local dev="$2"
    printf "%02X%02X%02X0000000000" "$((cmd & 0xFF))" "$(((cmd >> 8) & 0xFF))" "$((dev & 0xFF))"
}

spaced_payload_prefix() {
    local compact="$1"
    local count="$2"
    local out=""
    local i
    for ((i = 0; i < count * 2; i += 2)); do
        if [[ -n "$out" ]]; then
            out+=" "
        fi
        out+="${compact:i:2}"
    done
    printf "%s" "$out"
}

require_tool() {
    command -v "$1" >/dev/null 2>&1 || {
        echo "Missing required tool: $1" >&2
        exit 1
    }
}

setup_can() {
    sudo ip link set "$IFACE" down 2>/dev/null || true
    sudo ip link set "$IFACE" type can bitrate "$BITRATE" sample-point "$SAMPLE_POINT" restart-ms 100 loopback off
    sudo ip link set "$IFACE" up
    ip -details -statistics link show "$IFACE"
}

capture_send() {
    local can_id="$1"
    local payload="$2"
    local cmd="$3"
    local dev="$4"
    local tmp
    local candump_pid
    local ack_id
    local nack_id
    local done_id
    local ack_prefix
    local done_prefix

    tmp="$(mktemp)"
    ack_id="$(build_id "$PRIO_NORMAL" "$MSG_ACK" "$(conductor_dec)" "$(node_dec)")"
    nack_id="$(build_id "$PRIO_NORMAL" "$MSG_NACK" "$(conductor_dec)" "$(node_dec)")"
    done_id="$(build_id "$PRIO_NORMAL" "$MSG_DATA_DONE_LOG" "$(conductor_dec)" "$(node_dec)")"

    echo "TX ${can_id}#${payload}"
    stdbuf -oL candump -x -t a "$IFACE" >"$tmp" &
    candump_pid="$!"
    sleep 0.08

    cansend "$IFACE" "${can_id}#${payload}"
    sleep "$CAPTURE_SEC"

    kill "$candump_pid" 2>/dev/null || true
    wait "$candump_pid" 2>/dev/null || true

    cat "$tmp"

    ack_prefix="$(spaced_payload_prefix "$payload" 2)"
    done_prefix="$(printf "01 %02X %02X %02X" "$((cmd & 0xFF))" "$(((cmd >> 8) & 0xFF))" "$((dev & 0xFF))")"

    if grep -q "${nack_id}.*\\[8\\].*${ack_prefix}" "$tmp"; then
        echo "RESULT: NACK received"
        rm -f "$tmp"
        return 2
    fi

    if ! grep -q "${ack_id}.*\\[8\\].*${ack_prefix}" "$tmp"; then
        echo "RESULT: ACK not found"
        rm -f "$tmp"
        return 1
    fi

    if ! grep -q "${done_id}.*\\[8\\].*${done_prefix}" "$tmp"; then
        echo "RESULT: DONE not found"
        rm -f "$tmp"
        return 1
    fi

    echo "RESULT: ACK and DONE received"
    rm -f "$tmp"
}

send_command() {
    local cmd="$1"
    local dev="$2"
    local can_id
    local payload

    can_id="$(build_id 0 "$MSG_COMMAND" "$(node_dec)" "$(conductor_dec)")"
    payload="$(cmd_payload "$cmd" "$dev")"
    capture_send "$can_id" "$payload" "$cmd" "$dev"
}

check_range() {
    local value="$1"
    local min="$2"
    local max="$3"
    local name="$4"

    if ! [[ "$value" =~ ^[0-9]+$ ]] || (( value < min || value > max )); then
        echo "${name} must be in range ${min}..${max}" >&2
        exit 1
    fi
}

main() {
    require_tool cansend
    require_tool candump
    require_tool stdbuf

    local action="${1:-}"
    case "$action" in
        setup)
            setup_can
            ;;
        info)
            send_command "$CMD_GET_DEVICE_INFO" 0
            ;;
        pump-start)
            check_range "${2:-}" 0 12 "pump index"
            send_command "$CMD_PUMP_START" "$2"
            ;;
        pump-stop)
            check_range "${2:-}" 0 12 "pump index"
            send_command "$CMD_PUMP_STOP" "$2"
            ;;
        pump-cycle)
            check_range "${2:-}" 0 12 "pump index"
            local delay_sec="${3:-1}"
            send_command "$CMD_PUMP_START" "$2"
            sleep "$delay_sec"
            send_command "$CMD_PUMP_STOP" "$2"
            ;;
        valve-open)
            check_range "${2:-}" 13 15 "valve index"
            send_command "$CMD_VALVE_OPEN" "$2"
            ;;
        valve-close)
            check_range "${2:-}" 13 15 "valve index"
            send_command "$CMD_VALVE_CLOSE" "$2"
            ;;
        send)
            local can_id="${2:-}"
            local payload="${3:-}"
            if ! [[ "$can_id" =~ ^[0-9A-Fa-f]{8}$ && "$payload" =~ ^[0-9A-Fa-f]{16}$ ]]; then
                usage
                exit 1
            fi
            echo "TX ${can_id^^}#${payload^^}"
            cansend "$IFACE" "${can_id^^}#${payload^^}"
            ;;
        *)
            usage
            exit 1
            ;;
    esac
}

main "$@"
