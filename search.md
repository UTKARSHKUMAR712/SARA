# SARA SEARCH PLUGIN (GO) - IMPLEMENTATION DIRECTIVE

## Goal

Build a lightweight web search and scraping plugin for SARA.

This must be a completely separate plugin.

DO NOT integrate search logic directly into sara_agent.exe.

Create:

```text
/plugins/search_plugin/
```

The plugin should compile into:

```text
search_plugin.exe
```

and communicate with SARA through IPC or localhost HTTP.

---

# Core Design Requirements

Priority:

1. Low RAM
2. Low CPU
3. Fast startup
4. Single executable
5. No Chromium
6. No Playwright
7. No Electron
8. No Python runtime

The plugin should remain lightweight and follow the same philosophy as PicoClaw/ZeroClaw.

---

# Language

Use:

```text
Go
```

---

# Required Libraries

Install:

```bash
go get github.com/gocolly/colly/v2
go get github.com/PuerkitoBio/goquery
```

Use:

* Colly for crawling
* GoQuery for parsing
* net/http
* encoding/json

Avoid unnecessary dependencies.

---

# Architecture

```text
SARA Agent
      ↓
Search Plugin
      ↓
DuckDuckGo Search
      ↓
Top Results
      ↓
Page Fetch
      ↓
Content Extraction
      ↓
JSON Response
      ↓
SARA
```

---

# Plugin Modes

## Mode 1: Search

Input:

```json
{
  "query":"latest unreal engine features"
}
```

Output:

```json
{
  "query":"latest unreal engine features",
  "results":[
    {
      "title":"...",
      "url":"...",
      "snippet":"..."
    }
  ]
}
```

---

## Mode 2: Search + Scrape

Input:

```json
{
  "query":"latest unreal engine features",
  "scrape":true
}
```

Output:

```json
{
  "query":"latest unreal engine features",
  "sources":[
    {
      "title":"...",
      "url":"...",
      "content":"..."
    }
  ]
}
```

---

# Search Engine

Use:

```text
DuckDuckGo
```

as default.

No API keys required.

No paid services.

No Google scraping initially.

---

# Content Extraction

When scraping:

Remove:

* ads
* navigation
* footer
* cookie banners
* sidebars
* comments

Extract:

* title
* main article text
* source URL

Return clean text only.

---

# Concurrency

Use Go goroutines.

Example:

```text
Search
↓
Top 5 URLs
↓
Fetch in parallel
↓
Extract in parallel
↓
Merge results
```

This should be significantly faster than sequential scraping.

---

# Domain Filtering

Prefer:

* wikipedia.org
* github.com
* docs sites
* official websites
* government sites
* major news sites

Avoid:

* pinterest.com
* facebook.com
* random spam sites

Create:

```text
trusted_domains.json
blocked_domains.json
```

for future customization.

---

# Resource Targets

Target:

```text
Idle:
5-15 MB RAM
```

```text
Searching:
10-30 MB RAM
```

Avoid large memory allocations.

Avoid loading unnecessary content.

---

# Runtime Model

Plugin should NOT remain running permanently.

Preferred flow:

```text
User asks search
        ↓
Launch search_plugin.exe
        ↓
Search
        ↓
Return JSON
        ↓
Exit
```

Resource usage when idle:

```text
0 MB
```
when use of this complete close that exe 
---

# SARA Integration

Create plugin interface:

```json
{
  "plugin":"search",
  "action":"search",
  "query":"who is prime minister of india"
}
```

Response:

```json
{
  "success":true,
  "answer":"Narendra Modi",
  "sources":[...]
}
```

---

# Future Features (Do NOT Implement Yet)

Future roadmap:

* Brave Search
* Multi-engine search
* AI summarization
* Search ranking
* Caching
* RSS feeds
* News mode
* Academic mode
* Browser fallback

---

# Important Rule

DO NOT use:

* Playwright
* Rod
* Chromium
* Electron
* Selenium
* Puppeteer

unless explicitly enabled in a future fallback mode.

The default plugin must remain:

```text
Lightweight
Fast
Low RAM
Low CPU
Single EXE
```

This plugin should feel like a native utility, not a browser automation system.
