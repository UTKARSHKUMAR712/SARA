package main

import (
	"encoding/json"
	"os"
	"strings"
)

var (
	blockedDomains []string
	trustedDomains []string
)

func init() {
	// Try to load from files — silently ignore if not present
	blockedDomains = loadDomainList("blocked_domains.json")
	trustedDomains = loadDomainList("trusted_domains.json")

	// Hard-coded defaults if not loaded
	if len(blockedDomains) == 0 {
		blockedDomains = []string{
			"pinterest.com",
			"pinterest.co.uk",
			"pinterest.in",
			"facebook.com",
			"instagram.com",
			"tiktok.com",
			"twitter.com",
			"x.com",
			"quora.com",
			"amazon.com",
			"flipkart.com",
			"ebay.com",
			"etsy.com",
			"shopify.com",
			"yelp.com",
			"tripadvisor.com",
		}
	}
}

func loadDomainList(filename string) []string {
	data, err := os.ReadFile(filename)
	if err != nil {
		return nil
	}
	var domains []string
	if err := json.Unmarshal(data, &domains); err != nil {
		return nil
	}
	return domains
}

// IsBlockedDomain returns true if the URL matches a blocked domain
func IsBlockedDomain(rawURL string) bool {
	rawURL = strings.ToLower(rawURL)
	for _, d := range blockedDomains {
		if strings.Contains(rawURL, strings.ToLower(d)) {
			return true
		}
	}
	return false
}

// IsTrustedDomain returns true if the URL matches a trusted/preferred domain
func IsTrustedDomain(rawURL string) bool {
	rawURL = strings.ToLower(rawURL)
	for _, d := range trustedDomains {
		if strings.Contains(rawURL, strings.ToLower(d)) {
			return true
		}
	}
	return false
}
