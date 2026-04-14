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
