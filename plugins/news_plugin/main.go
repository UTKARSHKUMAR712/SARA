package main

import (
	"encoding/json"
	"fmt"
	"io"
	"os"
	"strings"

	"github.com/mmcdole/gofeed"
)

// NewsRequest is the input from SARA agent
type NewsRequest struct {
	Plugin   string `json:"plugin"`
	Action   string `json:"action"`
	Category string `json:"category"`
	MaxItems int    `json:"max_items"`
}

// NewsArticle represents a single news item
type NewsArticle struct {
	Title       string `json:"title"`
	URL         string `json:"url"`
	Description string `json:"description,omitempty"`
	Source      string `json:"source,omitempty"`
	PublishedAt string `json:"published_at,omitempty"`
}

// PluginResponse is the JSON written to stdout
type PluginResponse struct {
	Success  bool          `json:"success"`
	Category string        `json:"category"`
	Error    string        `json:"error,omitempty"`
	Articles []NewsArticle `json:"articles,omitempty"`
}

var feedURLs = map[string]string{
	"india":         "https://news.google.com/rss/headlines/section/geo/IN?hl=en-IN&gl=IN&ceid=IN:en",
	"world":         "https://news.google.com/rss/headlines/section/topic/WORLD?hl=en-IN&gl=IN&ceid=IN:en",
	"technology":    "https://news.google.com/rss/headlines/section/topic/TECHNOLOGY?hl=en-IN&gl=IN&ceid=IN:en",
	"business":      "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=en-IN&gl=IN&ceid=IN:en",
	"sports":        "https://news.google.com/rss/headlines/section/topic/SPORTS?hl=en-IN&gl=IN&ceid=IN:en",
	"entertainment": "https://news.google.com/rss/headlines/section/topic/ENTERTAINMENT?hl=en-IN&gl=IN&ceid=IN:en",
	"science":       "https://news.google.com/rss/headlines/section/topic/SCIENCE?hl=en-IN&gl=IN&ceid=IN:en",
	"health":        "https://news.google.com/rss/headlines/section/topic/HEALTH?hl=en-IN&gl=IN&ceid=IN:en",
	"general":       "https://news.google.com/rss?hl=en-IN&gl=IN&ceid=IN:en",
}

func main() {
	var raw string

	if len(os.Args) >= 2 {
		raw = strings.Join(os.Args[1:], " ")
	} else {
		data, err := io.ReadAll(os.Stdin)
		if err != nil || len(data) == 0 {
			printError("", "Usage: news_plugin.exe '<json_request>' or pipe JSON via stdin")
			os.Exit(1)
		}
		raw = string(data)
	}

	raw = strings.TrimSpace(raw)

	var req NewsRequest
	if err := json.Unmarshal([]byte(raw), &req); err != nil {
		printError("", "Invalid JSON: "+err.Error())
		os.Exit(1)
	}

	cat := strings.ToLower(req.Category)
	if cat == "" {
		cat = "general"
	}
	
	feedURL, ok := feedURLs[cat]
	if !ok {
		// Fallback to general if category unknown
		feedURL = feedURLs["general"]
		cat = "general"
	}

	if req.MaxItems <= 0 {
		req.MaxItems = 10
	}
	if req.MaxItems > 20 {
		req.MaxItems = 20
	}

	// Fetch RSS
	fp := gofeed.NewParser()
	feed, err := fp.ParseURL(feedURL)
	if err != nil {
		printError(cat, "Failed to fetch news: "+err.Error())
		os.Exit(1)
	}

	var articles []NewsArticle
	for i, item := range feed.Items {
		if i >= req.MaxItems {
			break
		}
		
		// Clean up Google News specific formatting (Source is usually after ' - ')
		title := item.Title
		source := ""
		if idx := strings.LastIndex(title, " - "); idx != -1 {
			source = title[idx+3:]
			title = title[:idx]
		}

		desc := item.Description
		desc = strings.ReplaceAll(desc, "&nbsp;", " ")
		desc = strings.ReplaceAll(desc, "&quot;", "\"")
		// Very simple HTML strip
		for {
			start := strings.Index(desc, "<")
			if start == -1 {
				break
			}
			end := strings.Index(desc[start:], ">")
			if end == -1 {
				break
			}
			desc = desc[:start] + desc[start+end+1:]
		}
		desc = strings.TrimSpace(desc)
		if len(desc) > 300 {
			desc = desc[:300] + "..."
		}

		articles = append(articles, NewsArticle{
			Title:       title,
			URL:         item.Link,
			Description: desc,
			Source:      source,
			PublishedAt: item.Published,
		})
	}

	resp := PluginResponse{
		Success:  true,
		Category: cat,
		Articles: articles,
	}

	data, _ := json.MarshalIndent(resp, "", "  ")
	fmt.Println(string(data))
}

func printError(category, msg string) {
	resp := PluginResponse{
		Success:  false,
		Category: category,
		Error:    msg,
	}
	data, _ := json.MarshalIndent(resp, "", "  ")
	fmt.Println(string(data))
}
