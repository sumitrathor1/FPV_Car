<?php
header("Access-Control-Allow-Origin: *");
header("Access-Control-Allow-Methods: GET, POST, OPTIONS");
header("Access-Control-Allow-Headers: Content-Type");

if ($_SERVER['REQUEST_METHOD'] === 'OPTIONS') {
    http_response_code(204);
    exit;
}

$file = __DIR__ . "/state.txt";

$data = [
    "cmd" => "S",
    "mode" => "0",
    "cam" => "1",
    "flash" => "0",
    "ai" => "0",
    "ai_obstacle" => "0",
    "ai_action" => "IDLE",
    "ai_turn" => "-",
    "ai_brightness" => "0.0",
    "ai_edge" => "0.0",
    "ai_near" => "0.0",
    "ai_near_ema" => "0.0",
    "ai_vclose" => "0.0",
    "ai_pred_near" => "0.0",
    "ai_pred_risk" => "0.0",
    "ai_hit" => "0",
    "ai_cmd" => "S",
    "ai_score" => "0.0",
    "ai_latency" => "0",
    "ai_worker" => "0",
    "fs" => "255",
    "bs" => "255"
];

if (file_exists($file)) {
    $json = file_get_contents($file);
    $decoded = json_decode($json, true);
    if ($decoded) {
        $data = array_merge($data, $decoded);
    }
}

if (isset($_GET['cmd'])) {
    $allowed = ['F', 'B', 'L', 'R', 'S'];
    if (in_array($_GET['cmd'], $allowed, true)) {
        $data["cmd"] = $_GET['cmd'];
    }
}

if (isset($_GET['mode'])) {
    $mode = (string)$_GET['mode'];
    if (in_array($mode, ['0', '1', '2'], true)) {
        $data["mode"] = $mode;
    }
}

if (isset($_GET['cam'])) {
    $data["cam"] = ($_GET['cam'] === "0") ? "0" : "1";
}

if (isset($_GET['flash'])) {
    $data["flash"] = ($_GET['flash'] === "1") ? "1" : "0";
}

if (isset($_GET['ai'])) {
    $data["ai"] = ($_GET['ai'] === "1") ? "1" : "0";
}

if (isset($_GET['fs'])) {
    $fs = max(0, min(255, (int)$_GET['fs']));
    $data["fs"] = (string)$fs;
}

if (isset($_GET['bs'])) {
    $bs = max(0, min(255, (int)$_GET['bs']));
    $data["bs"] = (string)$bs;
}

if (isset($_GET['esp_hb'])) {
    $data["esp_hb"] = (string)time();
}

// AI telemetry fields (written by the AI vision bot)
$ai_keys = [
    "ai_worker", "ai_obstacle", "ai_action", "ai_turn",
    "ai_brightness", "ai_edge", "ai_near", "ai_near_ema",
    "ai_vclose", "ai_pred_near", "ai_pred_risk", "ai_hit",
    "ai_cmd", "ai_score", "ai_latency"
];
foreach ($ai_keys as $key) {
    if (isset($_GET[$key])) {
        // Sanitize string to alphanumeric and basic punctuation only
        $val = substr((string)$_GET[$key], 0, 32);
        $data[$key] = preg_replace('/[^a-zA-Z0-9_.\-]/', '', $val);
    }
}

file_put_contents($file, json_encode($data), LOCK_EX);
echo "OK";
