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
    "mode" => "0"
];

if (file_exists($file)) {
    $json = file_get_contents($file);
    $decoded = json_decode($json, true);

    if ($decoded) {
        $data = $decoded;
    }
}

if (isset($_GET['cmd'])) {
    $allowed = ['F', 'B', 'L', 'R', 'S'];
    if (in_array($_GET['cmd'], $allowed, true)) {
        $data["cmd"] = $_GET['cmd'];
    }
}

if (isset($_GET['mode'])) {
    $data["mode"] = ($_GET['mode'] === "1") ? "1" : "0";
}

file_put_contents($file, json_encode($data));

echo "OK";
