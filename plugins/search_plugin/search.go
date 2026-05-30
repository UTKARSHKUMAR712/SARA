package main

import (
	"fmt"
	"io"
	"net/http"
	"net/url"
	"strings"
	"time"

	"github.com/PuerkitoBio/goquery"
)

// SearchDuckDuckGo searches DuckDuckGo HTML and returns results
func SearchDuckDuckGo(query string, maxResults int) ([]SearchResult, error) {
	// DuckDuckGo HTML search (lite endpoint — very lightweight)
	searchURL := "https://html.duckduckgo.com/html/?q=" + url.QueryEscape(query)

	client := &http.Client{
		Timeout: 15 * time.Second,
	}

	req, err := http.NewRequest("GET", searchURL, nil)
	if err != nil {
		return nil, fmt.Errorf("failed to build request: %w", err)
	}

	// Mimic a normal browser request so DDG doesn't block us
	req.Header.Set("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36")
	req.Header.Set("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8")
	req.Header.Set("Accept-Language", "en-US,en;q=0.5")

	resp, err := client.Do(req)
	if err != nil {
		return nil, fmt.Errorf("HTTP request failed: %w", err)
	}
	defer resp.Body.Close()

	if resp.StatusCode != 200 {
		return nil, fmt.Errorf("HTTP %d from DuckDuckGo", resp.StatusCode)
	}

	doc, err := goquery.NewDocumentFromReader(resp.Body)
	if err != nil {
		return nil, fmt.Errorf("failed to parse HTML: %w", err)
	}

	var results []SearchResult

	// DuckDuckGo HTML lite uses .result__body divs
	doc.Find(".result").Each(func(i int, s *goquery.Selection) {
		if len(results) >= maxResults {
			return
		}

		titleEl := s.Find(".result__title a")
		snippetEl := s.Find(".result__snippet")

		title := strings.TrimSpace(titleEl.Text())
		snippet := strings.TrimSpace(snippetEl.Text())

		href, exists := titleEl.Attr("href")
		if !exists || href == "" {
			return
		}

		// DDG wraps URLs — unwrap them
		finalURL := unwrapDDGURL(href)
		if finalURL == "" || strings.HasPrefix(finalURL, "javascript") {
			return
		}

		if title == "" {
			return
		}
		if IsBlockedDomain(finalURL) {
			return
		}

		results = append(results, SearchResult{
			Title:   title,
			URL:     finalURL,
			Snippet: snippet,
		})
	})

	return results, nil
}

// unwrapDDGURL extracts the real URL from DuckDuckGo's redirect URL
func unwrapDDGURL(href string) string {
	// DDG HTML lite sometimes gives direct URLs or "/l/?uddg=..." redirects
	if strings.HasPrefix(href, "http://") || strings.HasPrefix(href, "https://") {
		return href
	}

	// Parse as URL to extract uddg param
	parsed, err := url.Parse("https://html.duckduckgo.com" + href)
	if err != nil {
		return href
	}

	uddg := parsed.Query().Get("uddg")
	if uddg != "" {
		decoded, err := url.QueryUnescape(uddg)
		if err == nil {
			return decoded
		}
		return uddg
	}

	return href
}

// FetchPage downloads a URL and returns its raw body
func FetchPage(pageURL string) (io.ReadCloser, error) {
	client := &http.Client{
		Timeout: 12 * time.Second,
	}

	req, err := http.NewRequest("GET", pageURL, nil)
	if err != nil {
		return nil, err
	}

	req.Header.Set("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36")
	req.Header.Set("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8")

	resp, err := client.Do(req)
	if err != nil {
		return nil, err
	}

	if resp.StatusCode != 200 {
		resp.Body.Close()
		return nil, fmt.Errorf("HTTP %d", resp.StatusCode)
	}

	return resp.Body, nil
}
