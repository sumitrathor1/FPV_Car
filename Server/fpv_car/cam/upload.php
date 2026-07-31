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
    file_put_contents("$baseDir/latest.jpg", $data);

    $recordFile = "$baseDir/record.txt";
    if (file_exists($recordFile)) {
        $folder = trim(file_get_contents($recordFile));
        if ($folder !== "" && $folder !== "0") {
            $folderPath = "$baseDir/$folder";
            if (!is_dir($folderPath)) {
                mkdir($folderPath, 0755, true);
            }
            $timestamp = time() . "_" . microtime(true);
            file_put_contents("$folderPath/$timestamp.jpg", $data);
        }
    }
}

echo "OK";
