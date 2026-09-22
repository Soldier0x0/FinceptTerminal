# Local-Only Mode

This fork of Fincept Terminal builds in **local-only mode** by default. The
terminal starts on the dashboard, never contacts `api.fincept.in`, uses a
free LLM, and shows none of the Enterprise / pricing UI.

## What changes

| Area | Upstream | Local-only |
|---|---|---|
| Startup | Login → PIN → pricing gate → dashboard | Dashboard |
| Fincept account, credits, plan | Required for the dashboard | Not used; toolbar shows `LOCAL` |
| Default LLM | Fincept LLM (paid credits) | Ollama at `http://localhost:11434`, model `llama3.1:8b` |
| Fincept Cloud Sync | Settings → Cloud Sync | Hidden, never initialised |
| QuantLib cloud API (`/quantlib/*`) | Paid | Disabled — calls return "QuantLib cloud API is disabled in local-only mode." (a local backend is planned) |
| Update check | Upstream release feed | Off |
| Enterprise promo / UPGRADE button | Shown | Hidden |
| PIN lock | Available | Not available (it is tied to a Fincept session) |

Everything else — Yahoo Finance, FRED, NSE/BSE data, brokers, watchlists,
portfolio, notes, agents, every other LLM provider — is unchanged.

## Turning it off

Build-time (restores upstream behaviour):

```bash
cmake --preset linux-release -DFINCEPT_LOCAL_ONLY=OFF
```

Run-time (same binary, one launch):

```bash
FINCEPT_LOCAL_ONLY=0 ./FinceptTerminal
```

`FINCEPT_LOCAL_ONLY=1` forces it on for a binary built with the option off.

## Setting up a free LLM

### Ollama (local, no key, uses your GPU)

```bash
curl -fsSL https://ollama.com/install.sh | sh
ollama pull llama3.1:8b
systemctl status ollama      # should be active (running)
```

Open Settings → LLM Config. Ollama is already the active provider; the model
list is fetched live from `http://localhost:11434/api/tags`, so any model you
`ollama pull` appears there. On a 12 GB GPU `llama3.1:8b`, `qwen2.5:14b` and
`mistral-nemo:12b` all fit.

### Groq (cloud, free tier, fast)

1. Create a key at https://console.groq.com/keys
2. Settings → LLM Config → provider **Groq** → paste the key → pick
   `llama-3.3-70b-versatile` → Set Active.

Any other provider in the list (OpenAI, Anthropic, Gemini, OpenRouter, …)
works the same way with its own key.

## Troubleshooting

- **Chat says connection refused** — Ollama is not running: `systemctl start ollama`.
- **Model list is empty** — nothing pulled yet: `ollama pull llama3.1:8b`.
- **A valuation tab says "QuantLib cloud API is disabled"** — expected until
  the local QuantLib backend lands; the rest of the screen still works.
- **You want the upstream login flow back** — see "Turning it off".
