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
    "cam" => "1"
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
