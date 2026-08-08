const fileInput = document.getElementById("fileInput");
const fileInfo = document.getElementById("fileInfo");
const video = document.getElementById("video");
const image = document.getElementById("image");
const canvas = document.getElementById("overlay");
const modelStatus = document.getElementById("modelStatus");
const objectPresent = document.getElementById("objectPresent");
const totalObjects = document.getElementById("totalObjects");
const topMatch = document.getElementById("topMatch");
const labelsList = document.getElementById("labels");

const ctx = canvas.getContext("2d");

let model = null;
let frameLoopId = null;

init();

async function init() {
  try {
    model = await cocoSsd.load();
    modelStatus.textContent = "Model ready";
  } catch (error) {
    modelStatus.textContent = "Model failed to load";
    console.error(error);
  }
}

fileInput.addEventListener("change", (event) => {
  const file = event.target.files?.[0];
  if (!file) {
    return;
  }

  fileInfo.textContent = `${file.name} (${(file.size / 1024 / 1024).toFixed(2)} MB)`;
  clearVideoLoop();
  resetResults();

  const objectUrl = URL.createObjectURL(file);

  if (file.type.startsWith("image/")) {
    setupImage(objectUrl);
    return;
  }

  if (file.type.startsWith("video/")) {
    setupVideo(objectUrl);
    return;
  }

  modelStatus.textContent = "Unsupported file type";
});

function setupImage(src) {
  video.classList.add("hidden");
  image.classList.remove("hidden");
  image.src = src;

  image.onload = async () => {
    syncCanvasSize(image);
    await runDetection(image);
  };
}

function setupVideo(src) {
  image.classList.add("hidden");
  video.classList.remove("hidden");
  video.src = src;

  video.onloadedmetadata = () => {
    syncCanvasSize(video);
  };

  video.onplay = () => {
    runVideoLoop();
  };

  video.onpause = () => {
    clearVideoLoop();
  };

  video.onended = () => {
    clearVideoLoop();
  };
}

function syncCanvasSize(media) {
  canvas.width = media.clientWidth;
  canvas.height = media.clientHeight;
}

async function runDetection(element) {
  if (!model) {
    modelStatus.textContent = "Wait until model is ready";
    return;
  }

  const predictions = await model.detect(element);
  drawBoxes(predictions, element);
  updateResultCards(predictions);
}

function runVideoLoop() {
  clearVideoLoop();

  const detectFrame = async () => {
    if (video.paused || video.ended) {
      return;
    }

    syncCanvasSize(video);
    await runDetection(video);
    frameLoopId = window.setTimeout(detectFrame, 350);
  };

  detectFrame();
}

function clearVideoLoop() {
  if (frameLoopId) {
    window.clearTimeout(frameLoopId);
    frameLoopId = null;
  }
}

function drawBoxes(predictions, media) {
  ctx.clearRect(0, 0, canvas.width, canvas.height);

  const scaleX = canvas.width / media.videoWidth || canvas.width / media.naturalWidth || 1;
  const scaleY = canvas.height / media.videoHeight || canvas.height / media.naturalHeight || 1;

  predictions.forEach((prediction) => {
    const [x, y, width, height] = prediction.bbox;

    ctx.strokeStyle = "#ff6b35";
    ctx.lineWidth = 3;
    ctx.strokeRect(x * scaleX, y * scaleY, width * scaleX, height * scaleY);

    const text = `${prediction.class} ${(prediction.score * 100).toFixed(1)}%`;
    ctx.fillStyle = "rgba(19, 35, 47, 0.85)";
    ctx.fillRect(x * scaleX, y * scaleY - 24, text.length * 8.8, 22);
    ctx.fillStyle = "#ffffff";
    ctx.font = "14px Space Grotesk";
    ctx.fillText(text, x * scaleX + 5, y * scaleY - 8);
  });
}

function updateResultCards(predictions) {
  const uniqueLabels = [...new Set(predictions.map((item) => item.class))];

  objectPresent.textContent = predictions.length > 0 ? "Yes" : "No";
  totalObjects.textContent = String(predictions.length);
  topMatch.textContent = predictions[0]
    ? `${predictions[0].class} (${(predictions[0].score * 100).toFixed(1)}%)`
    : "-";

  labelsList.innerHTML = "";

  if (uniqueLabels.length === 0) {
    const li = document.createElement("li");
    li.textContent = "No object detected";
    labelsList.appendChild(li);
    return;
  }

  uniqueLabels.forEach((label) => {
    const li = document.createElement("li");
    li.textContent = label;
    labelsList.appendChild(li);
  });
}

function resetResults() {
  ctx.clearRect(0, 0, canvas.width, canvas.height);
  objectPresent.textContent = "-";
  totalObjects.textContent = "0";
  topMatch.textContent = "-";
  labelsList.innerHTML = "";
}
