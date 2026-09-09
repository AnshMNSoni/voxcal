# Google Calendar - Delete Event Tool Configuration

## Overview

The Delete Event tool permanently cancels and removes scheduled events from the user's Google Calendar upon explicit user request.

## n8n Node Configuration

- **Node Type**: Google Calendar Tool
- **Resource**: `Event`
- **Operation**: `Delete`
- **Connection**: Connected as a Tool to the AI Agent node

## Tool Description

```text
Delete an existing event from the user's Google Calendar.

Use this tool when the user explicitly asks to delete or cancel a calendar event.

If the event ID is not known, first search the calendar for the requested event. Never invent an event ID.
```

## Parameter Description

### Event ID Description

```text
The unique ID of the Google Calendar event to delete.

Never invent an Event ID.

If the user provides an Event ID, use that Event ID.

If the user identifies an event by its title, date, or other description, first use the Search Calendar Events tool to find the matching event and obtain its real Event ID. Then use that Event ID to delete the event.

Only delete the event after a matching event has been found.
```
