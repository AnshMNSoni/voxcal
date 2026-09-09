# Google Calendar - Update Event Tool Configuration

## Overview

The Update Event tool modifies existing calendar events (such as shifting times, adjusting titles, or changing dates) based on user instructions.

## n8n Node Configuration

- **Node Type**: Google Calendar Tool
- **Resource**: `Event`
- **Operation**: `Update`
- **Connection**: Connected as a Tool to the AI Agent node

## Tool Description

```text
Update an existing event in the user's Google Calendar.

Use this tool when the user asks to change an existing calendar event.

Before using this tool:
- If the user provides an Event ID, use that Event ID.
- If the user identifies the event by title, date, time, or description but does not provide an Event ID, first use the Get many events in Google Calendar tool to find the matching event.
- Use the real Event ID returned by the calendar search.
- Never invent an Event ID.

When updating an event, only change the fields requested by the user. Preserve other existing event information whenever possible.
```

## Parameter Descriptions

### Event ID Description

```text
The unique ID of the Google Calendar event to update.

Never invent an Event ID. Use an Event ID returned by a calendar search or provided directly by the user.
```

### Start Description

```text
The new start date and time of the event. Use Asia/Kolkata timezone unless the user explicitly specifies another timezone. Convert relative dates such as today and tomorrow using the current date and time.
```

### End Description

```text
The new end date and time of the event. Use Asia/Kolkata timezone unless the user explicitly specifies another timezone. If the user changes the start time but does not specify a new end time or duration, preserve the event's existing duration when possible.
```

### Summary Description

```text
The new title of the event, if the user wants to change the event title. Only provide a new title when the user requests a title change.
```
