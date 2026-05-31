package main

import (
	"encoding/json"
	"fmt"
	"io"
	"os"
	"strings"
)

// SearchRequest is the input from SARA agent
type SearchRequest struct {
	Plugin  string `json:"plugin"`
	Action  string `json:"action"`
	Query   string `json:"query"`
	Scrape  bool   `json:"scrape"`
	MaxURLs int    `json:"max_urls"`
}

// SearchResult is one search result
type SearchResult struct {
	Title   string `json:"title"`
	URL     string `json:"url"`
	Snippet string `json:"snippet"`
}

// ScrapeResult is one scraped page
type ScrapeResult struct {
	Title   string `json:"title"`
	URL     string `json:"url"`
	Content string `json:"content"`
}

// PluginResponse is the JSON written to stdout
type PluginResponse struct {
	Success bool           `json:"success"`
	Query   string         `json:"query"`
	Error   string         `json:"error,omitempty"`
	// Search-only mode
	Results []SearchResult `json:"results,omitempty"`
	// Scrape mode
	Sources []ScrapeResult `json:"sources,omitempty"`
	// Simple answer (best effort)
	Answer string `json:"answer,omitempty"`
}

func main() {
	var raw string

	if len(os.Args) >= 2 {
		// Rejoin all args (PowerShell may split on spaces)
		raw = strings.Join(os.Args[1:], " ")
	} else {
		// Read from stdin as fallback
		data, err := io.ReadAll(os.Stdin)
		if err != nil || len(data) == 0 {
			printError("", "Usage: search_plugin.exe '<json_request>' or pipe JSON via stdin")
			os.Exit(1)
		}
		raw = string(data)
	}

	raw = strings.TrimSpace(raw)

	var req SearchRequest
	if err := json.Unmarshal([]byte(raw), &req); err != nil {
		printError("", "Invalid JSON: "+err.Error()+" (got: "+raw+")")
		os.Exit(1)
	}

	if req.Query == "" {
		printError("", "Empty query")
		os.Exit(1)
	}

	if req.Action == "scrape" || req.Action == "search_plugin_scrape" {
		req.Scrape = true
	}

	if req.MaxURLs <= 0 {
		req.MaxURLs = 5
	}
	if req.MaxURLs > 10 {
		req.MaxURLs = 10
	}

	// Execute search
	results, err := SearchDuckDuckGo(req.Query, req.MaxURLs)
	if err != nil {
		printError(req.Query, "Search failed: "+err.Error())
		os.Exit(1)
	}

	if !req.Scrape {
		// Mode 1: Search only
		resp := PluginResponse{
			Success: true,
			Query:   req.Query,
			Results: results,
		}
		if len(results) > 0 {
			resp.Answer = results[0].Snippet
		}
		writeResponse(resp)
		return
	}

	// Mode 2: Search + Scrape
	urls := make([]string, 0, len(results))
	for _, r := range results {
		if !IsBlockedDomain(r.URL) {
			urls = append(urls, r.URL)
		}
		if len(urls) >= req.MaxURLs {
			break
		}
	}

	sources := ScrapeURLsConcurrent(urls)

	resp := PluginResponse{
		Success: true,
		Query:   req.Query,
		Sources: sources,
	}
	if len(sources) > 0 {
		// Best answer: first 500 chars of first result
		content := sources[0].Content
		if len(content) > 500 {
			content = content[:500] + "..."
		}
		resp.Answer = content
	}
	writeResponse(resp)
}

func writeResponse(r PluginResponse) {
	data, _ := json.MarshalIndent(r, "", "  ")
	fmt.Println(string(data))
}

func printError(query, msg string) {
	resp := PluginResponse{
		Success: false,
		Query:   query,
		Error:   msg,
	}
	writeResponse(resp)
}
