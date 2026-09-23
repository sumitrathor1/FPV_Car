<?php
header("Access-Control-Allow-Origin: *");
header("Access-Control-Allow-Methods: GET, POST, OPTIONS");
header("Access-Control-Allow-Headers: Content-Type");
header("Cache-Control: no-store, no-cache, must-revalidate, max-age=0");
header("Pragma: no-cache");
header("Expires: 0");

if (isset($_SERVER['REQUEST_METHOD']) && $_SERVER['REQUEST_METHOD'] === 'OPTIONS') {
    http_response_code(204);
    exit;
}

$file = __DIR__ . "/state.txt";

$default = [
    "cmd" => "S",
    "mode" => "0",
    "cam" => "1",
    "flash" => "0",
    "fs" => "255",
    "bs" => "255",
    "esp_hb" => "0",
    "car_ip" => "",
    "horn" => "0"
];

if (file_exists($file)) {
    $raw = @file_get_contents($file);
    $decoded = json_decode($raw, true);
    $state = is_array($decoded) ? array_merge($default, $decoded) : $default;

    // Fail-Safe: If ESP32 heartbeat is older than 5s or missing, force cmd to STOP ('S')
    $hb = isset($state['esp_hb']) ? (int)$state['esp_hb'] : 0;
    if ($hb === 0 || (time() - $hb) > 5) {
        $state['cmd'] = 'S';
    }

    echo json_encode($state);
} else {
    echo json_encode($default);
}
