<?php
header("Access-Control-Allow-Origin: *");
header("Access-Control-Allow-Methods: GET, POST, OPTIONS");
header("Access-Control-Allow-Headers: Content-Type");

if ($_SERVER['REQUEST_METHOD'] === 'OPTIONS') {
    http_response_code(204);
    exit;
}

$baseDir = __DIR__;
$data = file_get_contents("php://input");

if ($data && strlen($data) > 0) {
    file_put_contents("$baseDir/latest.jpg", $data, LOCK_EX);

    $recordFile = "$baseDir/record.txt";
    if (file_exists($recordFile)) {
        $folder = trim(file_get_contents($recordFile));
        if ($folder !== "" && $folder !== "0" && preg_match('/^rec_[a-zA-Z0-9_-]+$/', $folder)) {
            $folderPath = "$baseDir/$folder";
            if (!is_dir($folderPath)) {
                @mkdir($folderPath, 0755, true);
            }
            $timestamp = time() . "_" . round(microtime(true) * 1000);
            file_put_contents("$folderPath/$timestamp.jpg", $data, LOCK_EX);
        }
    }
}

echo "OK";
