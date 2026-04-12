<?php
header("Access-Control-Allow-Origin: *");
header("Access-Control-Allow-Methods: GET, POST, OPTIONS");
header("Access-Control-Allow-Headers: Content-Type");

if ($_SERVER['REQUEST_METHOD'] === 'OPTIONS') {
    http_response_code(204);
    exit;
}

$file = __DIR__ . "/state.txt";

if (file_exists($file)) {
    echo file_get_contents($file);
} else {
    echo json_encode([
        "cmd" => "S",
        "mode" => "0"
    ]);
}
