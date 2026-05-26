invoke_url='https://integrate.api.nvidia.com/v1/chat/completions'

payload=$(cat <<'JSON'
{
  "model": "deepseek-ai/deepseek-v4-flash",
  "messages": [{"role":"user","content":"Hello"}],
  "temperature": 1,
  "top_p": 0.95,
  "max_tokens": 16384,
  "chat_template_kwargs": {"thinking":true,"reasoning_effort":"high"},
  "stream": true
}
JSON
)

curl -sS -N \
  --request POST \
  --url "$invoke_url" \
  --header "Authorization: Bearer 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" \
  --header "Content-Type: application/json" \
  --data "$payload"
