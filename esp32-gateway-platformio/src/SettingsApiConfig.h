#pragma once

// Public REST endpoint for incubator settings (GET is unauthenticated; the
// backend serves defaults for unknown devices). See the IncubatorAWS repo,
// Lambda incubator-settings-get.
static constexpr char SETTINGS_API_HOST[]     = "b4ic8i07od.execute-api.eu-north-1.amazonaws.com";
static constexpr char SETTINGS_API_PATH_FMT[] = "/prod/settings/incubator-%02u";
