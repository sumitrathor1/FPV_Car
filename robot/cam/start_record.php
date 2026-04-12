<?php
header("Access-Control-Allow-Origin: *");
header("Access-Control-Allow-Methods: GET, POST, OPTIONS");
header("Access-Control-Allow-Headers: Content-Type");

if ($_SERVER['REQUEST_METHOD'] === 'OPTIONS') {
    http_response_code(204);
    exit;
}

$folder = "rec_" . date("Ymd_His");
$recDir = __DIR__ . "/" . $folder;

if (!is_dir($recDir)) {
        mkdir($recDir);
}

$indexFile = <<<'PHP'
<?php
$images = glob(__DIR__ . '/*.jpg');
sort($images);
?><!doctype html>
<html lang="en">
<head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>Recording Viewer</title>
    <style>
        body { margin: 0; padding: 16px; background: #081620; color: #fff; font-family: Arial, sans-serif; }
        h1 { margin: 0 0 16px; font-size: 20px; }
        .grid { display: grid; gap: 12px; max-width: 980px; margin: 0 auto; }
        .item { border: 1px solid rgba(255,255,255,0.12); border-radius: 10px; overflow: hidden; background: rgba(255,255,255,0.04); }
        img { width: 100%; display: block; height: auto; }
        .empty { color: #a8bfd1; }
    </style>
</head>
<body>
    <div class="grid">
        <h1><?= htmlspecialchars(basename(__DIR__), ENT_QUOTES, 'UTF-8') ?></h1>
        <?php if (!$images): ?>
            <div class="empty">No images yet.</div>
        <?php endif; ?>
        <?php foreach ($images as $image): ?>
            <div class="item"><img src="<?= htmlspecialchars(basename($image), ENT_QUOTES, 'UTF-8') ?>" alt=""></div>
        <?php endforeach; ?>
    </div>
</body>
</html>
PHP;

file_put_contents($recDir . "/index.php", $indexFile);
file_put_contents(__DIR__ . "/record.txt", $folder);

echo $folder;
