<?php
header("Access-Control-Allow-Origin: *");
header("Access-Control-Allow-Methods: GET, POST, OPTIONS");
header("Access-Control-Allow-Headers: Content-Type");
header("Cache-Control: no-store, no-cache, must-revalidate, max-age=0");
header("Pragma: no-cache");
header("Expires: 0");

if ($_SERVER['REQUEST_METHOD'] === 'OPTIONS') {
    http_response_code(204);
    exit;
}

$file = __DIR__ . "/state.txt";

$default = [
    "cmd" => "S",
    "mode" => "0",
    "cam" => "1",
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
    $raw = file_get_contents($file);
    $decoded = json_decode($raw, true);
    if (is_array($decoded)) {
        echo json_encode(array_merge($default, $decoded));
    } else {
        echo json_encode($default);
    }
} else {
    echo json_encode($default);
}
