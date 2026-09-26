# Procedural item wants: Requests page

This is the Company Book review for #928 at save schema 121, rebased onto
main after #931 (schema 119, raw stone) and #929 (schema 120, cargo overflow
allowance).

## Evidence

- `requests-page.png`: a native `--capture-requests requests-page.png`
  capture. This is a small addition to `src/client/cc_capture_request.inc`
  (a `capture_requests` flag alongside `capture_board`) that opens the
  Company Book directly on the Requests page (`book_page 5`), since no
  existing capture option reached it.
- The frame shows the Company Book opened with the "Requests 6" tab active
  and highlighted, and the page's own empty state: "People and belongings /
  Ask people what they need. Their requests appear here. Collect belongings
  from town stores and leave carried items there." A fresh campaign has no
  active request yet, so this is the page's real resting state, not a staged
  fixture.
- All 248 CTest checks passed locally at schema 121, including the wants
  feature's own `procedural_item_wants` suite (12 direct tests).
