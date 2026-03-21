**Core architecture**

- Two queues, two mutexes — requests go in, results come out. Main thread never blocks.
- Only the main thread writes to app state. Network thread only touches the result queue.
- `tick_network` drain runs once per frame before your ImGui windows.

**TODOs in order**

1. Add `fetch_status` enum to `artist_node` (IDLE, LOADING, READY)
2. Define `fetch_request` and `fetch_result` structs — both carry the relevant id (artist_id, album_id) so you can match results back to the right place
3. Implement the network thread worker — your existing blocking curl + parse code moves in almost unchanged, just push a result at the end instead of writing to app state
4. Implement `tick_network` and call it once per frame
5. Port **build remote browser** first — simplest result handler, proves the whole pipeline works
6. Port **fetch artist albums** second — we already designed this exactly
7. Port **media view** and **validation** last — they can stay blocking for now without much pain

**Things to be careful of**

- The `albums.empty()` gate becomes a problem when you go async — replace it with the status enum so you don't push duplicate requests during the loading window
- Media view needs the `pending_album_id` pattern — only accept the result if it matches the last thing the user clicked, silently drop anything older
- The network thread must never write to `app_state.artists` directly — only through the result queue
- `curl_easy_cleanup` and `free(res.response)` must still happen on error paths in the worker, not just the happy path
- `simdjson::padded_string` copies the buffer, so you can `free(res.response)` right after constructing it — do this before parsing so you don't leak on a parse error
