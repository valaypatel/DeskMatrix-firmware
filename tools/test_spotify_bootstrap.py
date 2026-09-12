#!/usr/bin/env python3
# tools/test_spotify_bootstrap.py
import unittest
from spotify_bootstrap import build_authorize_url, extract_code_from_redirect


class TestSpotifyBootstrap(unittest.TestCase):
    def test_build_authorize_url_includes_required_params(self):
        url = build_authorize_url("abc123", "http://127.0.0.1:8888/callback")
        self.assertIn("client_id=abc123", url)
        self.assertIn("response_type=code", url)
        self.assertIn("redirect_uri=http%3A%2F%2F127.0.0.1%3A8888%2Fcallback", url)
        self.assertIn("scope=user-read-currently-playing", url)

    def test_extract_code_from_redirect_url(self):
        url = "http://127.0.0.1:8888/callback?code=AQD_someLongCode123&state=xyz"
        self.assertEqual(extract_code_from_redirect(url), "AQD_someLongCode123")

    def test_extract_code_missing_raises(self):
        with self.assertRaises(ValueError):
            extract_code_from_redirect("http://127.0.0.1:8888/callback?error=access_denied")


if __name__ == "__main__":
    unittest.main()
