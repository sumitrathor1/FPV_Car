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
    "ai" => "0",
    "ai_obstacle" => "0",
    "ai_action" => "IDLE",
    "ai_turn" => "-",
    "ai_brightness" => "0.0",
    "ai_edge" => "0.0",
    "ai_score" => "0.0",
    "ai_latency" => "0",
    "ai_worker" => "0",
    "fs" => "200",
    "bs" => "200"
];

if (file_exists($file)) {
    $json = file_get_contents($file);
    $decoded = json_decode($json, true);

    if ($decoded) {
        $data = array_merge($data, $decoded);
    }
}

$original = $data;

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

if (isset($_GET['ai'])) {
    $data["ai"] = ($_GET['ai'] === "1") ? "1" : "0";
}

if ($data["ai"] === "1") {
    $data["cam"] = "1";
}

if ($data["cam"] === "0") {
    $data["ai"] = "0";
    $data["ai_worker"] = "0";
    $data["ai_obstacle"] = "0";
    $data["ai_action"] = "CAMERA_OFF";
    $data["ai_turn"] = "-";
    $data["ai_brightness"] = "0.0";
    $data["ai_edge"] = "0.0";
    $data["ai_score"] = "0.0";
    $data["ai_latency"] = "0";
}

if (isset($_GET['ai_obstacle'])) {
    $data["ai_obstacle"] = ($_GET['ai_obstacle'] === "1") ? "1" : "0";
}

if (isset($_GET['ai_action'])) {
    $action = strtoupper(preg_replace('/[^A-Z0-9_\-]/i', '', (string)$_GET['ai_action']));
    $data["ai_action"] = substr($action !== "" ? $action : "IDLE", 0, 32);
}

if (isset($_GET['ai_turn'])) {
    $turn = strtoupper((string)$_GET['ai_turn']);
    $data["ai_turn"] = in_array($turn, ['L', 'R', '-'], true) ? $turn : '-';
}

if (isset($_GET['ai_brightness'])) {
    $data["ai_brightness"] = (string)round((float)$_GET['ai_brightness'], 1);
}

if (isset($_GET['ai_edge'])) {
    $data["ai_edge"] = (string)round((float)$_GET['ai_edge'], 1);
}

if (isset($_GET['ai_score'])) {
    $data["ai_score"] = (string)round((float)$_GET['ai_score'], 1);
}

if (isset($_GET['ai_latency'])) {
    $lat = max(0, min(5000, (int)$_GET['ai_latency']));
    $data["ai_latency"] = (string)$lat;
}

if (isset($_GET['ai_worker'])) {
    $data["ai_worker"] = ($_GET['ai_worker'] === "1") ? "1" : "0";
}

if (isset($_GET['cmd'])) {
    $allowed = ['F', 'B', 'L', 'R', 'S'];
    if (in_array($_GET['cmd'], $allowed, true)) {
        $cmd = $_GET['cmd'];
        if ($data["ai_obstacle"] === "1" && $cmd === 'F') {
            $data["cmd"] = 'S';
        } else {
            $data["cmd"] = $cmd;
        }
    }
}

if (isset($_GET['fs'])) {
    $fs = max(0, min(255, (int)$_GET['fs']));
    $data["fs"] = (string)$fs;
}

if (isset($_GET['bs'])) {
    $bs = max(0, min(255, (int)$_GET['bs']));
    $data["bs"] = (string)$bs;
}

if ($data !== $original || !file_exists($file)) {
    file_put_contents($file, json_encode($data), LOCK_EX);
}

echo "OK";
