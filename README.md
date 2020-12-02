# fast async rest http micro service


## Bugs

Usage of `http::response::write_headers` hit performance. I don't know why, so you should use
it only if you need some special processing of response.
