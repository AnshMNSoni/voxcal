# n8n User Message Template

## Overview

The User Message Template in the n8n AI Agent node formats the incoming payload from the ESP32 before passing it to the language model. It injects the authoritative current date, timestamp, and timezone dynamically into the context.

## Template

```text
CURRENT DATE: {{ $json.current_date }}
CURRENT DATE AND TIME: {{ $json.current_datetime }}
TIMEZONE: {{ $json.timezone }}

USER REQUEST:
{{ $json.body.message }}
```

## Field Explanations

- `{{ $json.current_date }}`: The dynamic current calendar date evaluated at the time the webhook is received.
- `{{ $json.current_datetime }}`: The full date and time string used by the agent to anchor start/end intervals.
- `{{ $json.timezone }}`: The user's active timezone (default: `Asia/Kolkata`).
- `{{ $json.body.message }}`: The raw command string sent by the ESP32 via the Gateway.

## Technical Purpose

Language models have no internal real-time clock. By explicitly prepending current date, time, and timezone metadata before the user request, the agent can reliably resolve relative temporal expressions (such as "today", "tomorrow", "this Friday at 4 PM") without hallucinating or defaulting to outdated training dates.
