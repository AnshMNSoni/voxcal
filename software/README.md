# VoxCal - n8n Workflow Automation

This directory contains the Docker setup for running [n8n](https://n8n.io/) locally for the VoxCal workflow backend.

## Prerequisites

- [Docker](https://www.docker.com/products/docker-desktop/) & Docker Compose installed and running.

---

## Getting Started

### 1. Environment Configuration
The `.env` file is pre-configured. To customize any settings (port, timezone, webhook URLs), modify `.env` or refer to `.env.example`:

```env
N8N_PORT=5678
GENERIC_TIMEZONE=Asia/Kolkata
WEBHOOK_URL=http://localhost:5678/
```

### 2. Start n8n
To start n8n in the background:

```powershell
docker compose up -d
```

### 3. Access n8n Web UI
Open your browser and navigate to:
[http://localhost:5678](http://localhost:5678)

On your first visit, you will be prompted to set up your owner/admin account.

---

## Management Commands

| Action | Command |
| :--- | :--- |
| **Start / Run** | `docker compose up -d` |
| **Stop** | `docker compose down` |
| **View Logs** | `docker compose logs -f` |
| **Restart** | `docker compose restart` |
| **Update Image** | `docker compose pull && docker compose up -d` |

---

## Data Persistence & Shared Files

- **Database & Credentials**: Stored securely in the Docker named volume `voxcal_n8n_data`.
- **Shared Files**: Files placed inside `./local_files` are mounted inside the container at `/data/shared`.
