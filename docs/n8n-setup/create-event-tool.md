# Google Calendar - Create Event Tool Configuration

## Overview

The Create Event tool enables the n8n AI Agent to schedule and persist new events in the user's Google Calendar.

## n8n Node Configuration

- **Node Type**: Google Calendar Tool
- **Resource**: `Event`
- **Operation**: `Create`
- **Connection**: Connected as a Tool to the AI Agent node

## Tool Purpose & Documentation

Create a new event in the user's Google Calendar when the user requests a calendar event to be created.

The AI Agent automatically provides the structured event details derived from the user request, including:
- **Title / Summary**: Event name or subject.
- **Start Time**: ISO 8601 formatted date-time string anchored to the authoritative current date and timezone.
- **End Time**: Calculated end timestamp (defaults to 1 hour duration if not specified by the user).
- **Calendar ID**: Target calendar (typically `primary`).

## Agent Interaction Workflow

1. The user issues a scheduling request (e.g., "Schedule a sprint review tomorrow at 3 PM").
2. The agent calculates the exact date and start/end timestamps based on `CURRENT DATE` and `TIMEZONE`.
3. The agent calls the Create Event tool with the structured parameters.
4. The tool executes the Google Calendar API call and returns the newly created event object and unique Event ID.
5. The agent formulates a spoken confirmation string for the ESP32.
