# fast async rest http micro service


## Notes

Usage of `http::response::write_headers` hit performance, so its not recomended
to use it (except case, when you completely understand that you do).
