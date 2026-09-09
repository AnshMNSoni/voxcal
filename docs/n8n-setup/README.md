# n8n Step-by-Step Configuration Guide

This directory contains the exact prompts, system instructions, and tool descriptions configured in the n8n ReAct Agent workflow for VoxCal.

## Table of Configuration Guides

1. [n8n User Message Template](user-prompt.md)
   Dynamic template injecting current date, timestamp, timezone, and raw ESP32 command.

2. [n8n AI Agent System Prompt](system-prompt.md)
   Authoritative instructions defining agent identity, relative date arithmetic rules, calendar integrity guards, and spoken output constraints.

3. [Create Event Tool Configuration](create-event-tool.md)
   Google Calendar create event tool setup, purpose, and automated parameter passing.

4. [Search Events Tool Configuration](search-events-tool.md)
   Google Calendar date/time range search tool description and role in resolving real Event IDs.

5. [Update Event Tool Configuration](update-event-tool.md)
   Google Calendar update event tool description and granular field parameter definitions (Event ID, Start, End, Summary).

6. [Delete Event Tool Configuration](delete-event-tool.md)
   Google Calendar delete event tool description and strict Event ID resolution rules.

## Architectural Context

In n8n, the AI Agent node acts as the reasoning controller. When a payload arrives via the Webhook node from the local Gateway:
- The **User Message Template** provides the temporal context.
- The **System Prompt** enforces reasoning rules and restricts operations to calendar actions.
- The **Google Calendar Tools** are called dynamically as needed to execute CRUD operations against the user's schedule.
