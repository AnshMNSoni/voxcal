# n8n AI Agent — User Message Template

## Overview

The User Message Template is configured in the **AI Agent** node's `text` field (with `promptType: define`).
It formats the incoming webhook payload into a structured context block before passing it to the language model.

The template is reproduced **verbatim** from the live n8n workflow export (`jVxIMtua9iTDiASX`, version `v0.1.54`,
exported 2026-09-29).

---

## Template

```text
CURRENT DATE: {{ $json.current_date }}
CURRENT DATE AND TIME: {{ $json.current_datetime }}
TIMEZONE: {{ $json.timezone }}

USER REQUEST:
{{ $json.body.message }}
```

> **Note:** This template references fields produced by the **Edit Fields** node that immediately precedes
> the AI Agent. The Edit Fields node computes `current_date`, `current_datetime`, and `timezone` from
> n8n's `$now` at execution time, and passes through `body` from the original webhook payload.

---

## How the Edit Fields Node Produces These Values

The **Edit Fields** node (n8n Set node, type `n8n-nodes-base.set`) in the Main workflow computes
the following fields before the AI Agent runs:

| Field | n8n Expression | Type | Description |
|:---|:---|:---|:---|
| `current_date` | `={{ $now.setZone('Asia/Kolkata').toFormat('yyyy-MM-dd') }}` | string | Calendar date at webhook receipt |
| `current_datetime` | `={{ $now.setZone('Asia/Kolkata').toFormat('yyyy-MM-dd HH:mm:ss') }}` | string | Full date-time at webhook receipt |
| `timezone` | `Asia/Kolkata` (literal) | string | Timezone label |
| `body` | `={{ $json.body }}` | object | Original webhook request body |

The AI Agent then reads `$json.current_date`, `$json.current_datetime`, `$json.timezone`, and
`$json.body.message` from the Edit Fields output.

---

## Field Explanations

- **`{{ $json.current_date }}`** — Calendar date (e.g. `2026-09-29`) computed by n8n from the
  system clock at the moment the webhook is received. Used by the agent to resolve relative dates
  such as "today", "tomorrow", and "yesterday".

- **`{{ $json.current_datetime }}`** — Full date and time string (e.g. `2026-09-29 19:44:26`) used
  by the agent to anchor start/end intervals when the user specifies a time of day.

- **`{{ $json.timezone }}`** — Timezone label (default: `Asia/Kolkata`). Preserved in all ISO 8601
  timestamps sent to Calendar Worker.

- **`{{ $json.body.message }}`** — The raw command string sent by the ESP32 via the Gateway, or
  the `message` field in a direct webhook test payload.

---

## Technical Purpose

Language models have no internal real-time clock. Without explicitly supplied date context, a model
may resolve "tomorrow" against a stale training-data date or refuse to compute dates at all.

By prepending `CURRENT DATE`, `CURRENT DATE AND TIME`, and `TIMEZONE` before every user request,
the agent can reliably resolve all relative temporal expressions against the actual execution time.

Additionally, because these values are computed by n8n (not sent by the ESP32), they are
authoritative and consistent even when the ESP32 does not supply its own timestamp.
The Gateway's `n8n_payload` also independently injects `current_date` and `current_datetime`, but
Edit Fields recomputes them server-side to prevent clock skew from client-sent values.

---

## Webhook Payload Contract

When testing the webhook directly (without the ESP32 and Gateway), the POST body must contain at
minimum a `body.message` field. The Edit Fields node overwrites `current_date`, `current_datetime`,
and `timezone` server-side, so these do not need to be accurate in the test payload — but supplying
them mirrors the Gateway's behavior.

Example minimal test payload:

```json
{
  "body": {
    "message": "Schedule a Team meeting tomorrow at 10 AM"
  },
  "device_id": "esp32-01",
  "type": "command"
}
```
