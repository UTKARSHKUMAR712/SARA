package main

import (
	"strings"
	"sync"
	"unicode"

	"github.com/PuerkitoBio/goquery"
)

// ScrapeURLsConcurrent fetches and extracts content from multiple URLs in parallel
func ScrapeURLsConcurrent(urls []string) []ScrapeResult {
	var wg sync.WaitGroup
	results := make([]ScrapeResult, len(urls))

	for i, u := range urls {
		wg.Add(1)
		go func(idx int, pageURL string) {
			defer wg.Done()
			result := scrapeURL(pageURL)
			results[idx] = result
		}(i, u)
	}

	wg.Wait()

	// Filter empty results
	var filtered []ScrapeResult
	for _, r := range results {
		if r.URL != "" && r.Content != "" {
			filtered = append(filtered, r)
		}
	}
	return filtered
}

// scrapeURL fetches a single page and extracts clean text
func scrapeURL(pageURL string) ScrapeResult {
	body, err := FetchPage(pageURL)
	if err != nil {
		return ScrapeResult{URL: pageURL}
	}
	defer body.Close()

	doc, err := goquery.NewDocumentFromReader(body)
	if err != nil {
		return ScrapeResult{URL: pageURL}
	}

	// Remove noise elements
	doc.Find("script, style, nav, footer, header, aside, form, iframe, noscript, " +
		".ad, .ads, .advertisement, .cookie, .sidebar, .comment, .comments, " +
		"#nav, #footer, #header, #sidebar, #comments, #ad, #ads, " +
		"[role=navigation], [role=banner], [role=contentinfo], [role=complementary]").Remove()

	title := strings.TrimSpace(doc.Find("title").First().Text())

	// Try to get main content first
	content := ""
	mainSel := doc.Find("main, article, [role=main], .content, .post-content, .article-body, .entry-content, #content, #main")
	if mainSel.Length() > 0 {
		content = extractText(mainSel.First())
	}

	// Fallback to body
	if content == "" || len(content) < 100 {
		content = extractText(doc.Find("body"))
	}

	// Cap at 25000 chars to avoid memory exhaustion on massive files
	if len(content) > 25000 {
		content = content[:25000] + "..."
	}

	return ScrapeResult{
		Title:   title,
		URL:     pageURL,
		Content: content,
	}
}

// extractText walks a goquery selection and extracts readable text
func extractText(sel *goquery.Selection) string {
	var sb strings.Builder
	sel.Find("p, h1, h2, h3, h4, li, td, th, blockquote, pre, code").Each(func(_ int, s *goquery.Selection) {
		text := strings.TrimSpace(s.Text())
		text = normalizeWhitespace(text)
		if len(text) > 20 { // skip tiny fragments
			sb.WriteString(text)
			sb.WriteString("\n")
		}
	})
	return strings.TrimSpace(sb.String())
}

// normalizeWhitespace collapses multiple spaces/newlines into single spaces
func normalizeWhitespace(s string) string {
	var sb strings.Builder
	prevSpace := false
	for _, r := range s {
		if unicode.IsSpace(r) {
			if !prevSpace {
				sb.WriteRune(' ')
			}
			prevSpace = true
		} else {
			sb.WriteRune(r)
			prevSpace = false
		}
	}
	return strings.TrimSpace(sb.String())
}
