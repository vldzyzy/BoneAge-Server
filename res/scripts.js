// DOM元素
const imageInput = document.getElementById('imageInput');
const selectButton = document.getElementById('selectButton');
const uploadButton = document.getElementById('uploadButton');
const drawButton = document.getElementById('drawButton');
const deleteButton = document.getElementById('deleteButton');
const clearUserBoxes = document.getElementById('clearUserBoxes');
const clearServerBoxes = document.getElementById('clearServerBoxes');
const clearAllBoxes = document.getElementById('clearAllBoxes');
const submitBoxes = document.getElementById('submitBoxes');
const messageArea = document.getElementById('messageArea');
const imageCanvas = document.getElementById('imageCanvas');
const legendSection = document.getElementById('legendSection');
const legend = document.getElementById('legend');
const spinner = document.getElementById('spinner');
const dropZone = document.getElementById('dropZone');
const tooltip = document.getElementById('tooltip');
const statusBar = document.getElementById('statusBar');
const fileInfo = document.getElementById('fileInfo');
const boxCount = document.getElementById('boxCount');
const serverBoxCount = document.getElementById('serverBoxCount');
const userBoxCount = document.getElementById('userBoxCount');
const boxInfo = document.getElementById('boxInfo');
const boxLabel = document.getElementById('boxLabel');
const boxCoords = document.getElementById('boxCoords');
const boxSize = document.getElementById('boxSize');
const boxSource = document.getElementById('boxSource');
const genderSelect = document.getElementById('genderSelect');
const boneAgeResult = document.getElementById('boneAgeResult');
const boneAgeValue = document.getElementById('boneAgeValue');
const resultsContainer = document.getElementById('resultsContainer');
const jointsGrid = document.getElementById('jointsGrid');
const totalScoreValue = document.getElementById('totalScoreValue');

// 上下文
const ctx = imageCanvas.getContext('2d');

// 状态变量
let originalImage = null;
let serverBoxes = [];  
let userBoxes = [];    
let allBoxes = [];     
let isDrawing = false;
let startX, startY;
let currentBox = null;
let drawingEnabled = false;
let selectedBoxIndex = -1;
let scale = 1;
let MAX_FILE_SIZE = 20 * 1024 * 1024;
let currentMaturityLevels = [];
let detectionMapping = {};
let totalScore = 0;

// 颜色常量
const SERVER_BOX_COLOR = '#1E90FF';
const USER_BOX_COLOR = '#FF4500';
const SELECTED_BOX_COLOR = '#FFD700';

// 映射检测框名称到分数键名
const DETECTION_TO_SCORE_MAP = {
    'DIP1': 'DIPFirst',
    'DIP3': 'DIPThird',
    'DIP5': 'DIPFifth',
    'PIP1': 'PIPFirst',
    'PIP3': 'PIPThird',
    'PIP5': 'PIPFifth',
    'MIP3': 'MIPThird',
    'MIP5': 'MIPFifth',
    'MCP3': 'MCPThird',
    'MCP5': 'MCPFifth',
    'FMCP': 'FMCP',
    'Radius': 'Radius',
    'Ulna': 'Ulna'
};

// 中文名称映射
const BONE_NAME_MAP = {
    'DIP1': '拇指远节指骨',
    'DIP3': '中指远节指骨',
    'DIP5': '小指远节指骨',
    'PIP1': '拇指近节指骨',
    'PIP3': '中指近节指骨',
    'PIP5': '小指近节指骨',
    'MIP3': '中指中节指骨',
    'MIP5': '小指中节指骨',
    'MCP3': '中指掌骨',
    'MCP5': '小指掌骨',
    'FMCP': '拇指掌骨',
    'Radius': '桡骨',
    'Ulna': '尺骨'
};

// 骨龄评分系统
const SCORE = {
    'girl':{
        'Radius':[10,15,22,25,40,59,91,125,138,178,192,199,203,210],
        'Ulna':[27,31,36,50,73,95,120,157,168,176,182,189],
        'FMCP':[5,7,10,16,23,28,34,41,47,53,66],
        'MCPThird':[3,5,6,9,14,21,32,40,47,51],
        'MCPFifth':[4,5,7,10,15,22,33,43,47,51],
        'PIPFirst':[6,7,8,11,17,26,32,38,45,53,60,67],
        'PIPThird':[3,5,7,9,15,20,25,29,35,41,46,51],
        'PIPFifth':[4,5,7,11,18,21,25,29,34,40,45,50],
        'MIPThird':[4,5,7,10,16,21,25,29,35,43,46,51],
        'MIPFifth':[3,5,7,12,19,23,27,32,35,39,43,49],
        'DIPFirst':[5,6,8,10,20,31,38,44,45,52,67],
        'DIPThird':[3,5,7,10,16,24,30,33,36,39,49],
        'DIPFifth':[5,6,7,11,18,25,29,33,35,39,49]
    },
    'boy':{
        'Radius':[8,11,15,18,31,46,76,118,135,171,188,197,201,209],
        'Ulna':[25,30,35,43,61,80,116,157,168,180,187,194],
        'FMCP':[4,5,8,16,22,26,34,39,45,52,66],
        'MCPThird':[3,4,5,8,13,19,30,38,44,51],
        'MCPFifth':[3,4,6,9,14,19,31,41,46,50],
        'PIPFirst':[4,5,7,11,17,23,29,36,44,52,59,66],
        'PIPThird':[3,4,5,8,14,19,23,28,34,40,45,50],
        'PIPFifth':[3,4,6,10,16,19,24,28,33,40,44,50],
        'MIPThird':[3,4,5,9,14,18,23,28,35,42,45,50],
        'MIPFifth':[3,4,6,11,17,21,26,31,36,40,43,49],
        'DIPFirst':[4,5,6,9,19,28,36,43,46,51,67],
        'DIPThird':[3,4,5,9,15,23,29,33,37,40,49],
        'DIPFifth':[3,4,6,11,17,23,29,32,36,40,49]
    }
};

// 初始化
function init() {
    legendSection.classList.add('hidden');
    
    // 事件监听器
    selectButton.addEventListener('click', () => imageInput.click());
    imageInput.addEventListener('change', handleImageSelect);
    uploadButton.addEventListener('click', uploadImage);
    drawButton.addEventListener('click', toggleDrawing);
    deleteButton.addEventListener('click', deleteSelectedBox);
    clearUserBoxes.addEventListener('click', () => clearBoxes('user'));
    clearServerBoxes.addEventListener('click', () => clearBoxes('server'));
    clearAllBoxes.addEventListener('click', () => clearBoxes('all'));
    submitBoxes.addEventListener('click', submitAllBoxes);
    genderSelect.addEventListener('change', recalculateBoneAge);
    
    // 拖放事件
    dropZone.addEventListener('dragover', handleDragOver);
    dropZone.addEventListener('dragleave', handleDragLeave);
    dropZone.addEventListener('drop', handleDrop);
    
    // 画布事件
    imageCanvas.addEventListener('mousedown', startDrawing);
    imageCanvas.addEventListener('mousemove', handleCanvasMouseMove);
    imageCanvas.addEventListener('mouseup', endDrawing);
    imageCanvas.addEventListener('mouseout', handleCanvasMouseOut);
    imageCanvas.addEventListener('click', handleCanvasClick);
    
    // 窗口大小变化时调整画布
    window.addEventListener('resize', () => {
        if (originalImage) {
            resizeCanvas();
            drawImage();
        }
    });
}

// 拖放处理
function handleDragOver(e) {
    e.preventDefault();
    e.stopPropagation();
    dropZone.classList.add('dragover');
}

function handleDragLeave(e) {
    e.preventDefault();
    e.stopPropagation();
    dropZone.classList.remove('dragover');
}

function handleDrop(e) {
    e.preventDefault();
    e.stopPropagation();
    dropZone.classList.remove('dragover');
    
    const dt = e.dataTransfer;
    const files = dt.files;
    
    if (files && files.length > 0) {
        const file = files[0];
        
        // 检查文件大小
        if (file.size > MAX_FILE_SIZE) {
            showMessage(`文件过大，请选择小于20MB的文件。当前文件大小: ${(file.size / (1024 * 1024)).toFixed(2)}MB`, 'error');
            return;
        }
        
        imageInput.files = files;
        handleImageSelect({ target: { files: files } });
    }
}

// 图片选择处理
function handleImageSelect(e) {
    const file = e.target.files[0];
    
    if (!file || !file.type.match('image.*')) {
        showMessage('请选择有效的图像文件', 'error');
        return;
    }
    
    if (file.size > MAX_FILE_SIZE) {
        showMessage(`文件过大，请选择小于20MB的文件。当前文件大小: ${(file.size / (1024 * 1024)).toFixed(2)}MB`, 'error');
        return;
    }
    
    const reader = new FileReader();
    
    reader.onload = function(event) {
        const img = new Image();
        img.onload = function() {
            originalImage = img;
            serverBoxes = [];
            userBoxes = [];
            allBoxes = [];
            selectedBoxIndex = -1;
            drawingEnabled = false;
            
            resizeCanvas();
            drawImage();
            uploadButton.disabled = false;
            clearMessage();
            
            drawButton.disabled = true;
            deleteButton.disabled = true;
            clearUserBoxes.disabled = true;
            clearServerBoxes.disabled = true;
            clearAllBoxes.disabled = true;
            submitBoxes.disabled = true;
            
            const fileSizeMB = (file.size / (1024 * 1024)).toFixed(2);
            fileInfo.textContent = `${file.name} (${img.width}x${img.height}, ${fileSizeMB}MB)`;
            statusBar.classList.remove('hidden');
            
            legendSection.classList.add('hidden');
            boxInfo.classList.add('hidden');
            boneAgeResult.classList.add('hidden');
            resultsContainer.classList.add('hidden');
        };
        img.src = event.target.result;
    };
    
    reader.readAsDataURL(file);
}

// 调整画布大小
function resizeCanvas() {
    if (originalImage) {
        const container = document.querySelector('.canvas-container');
        const maxWidth = container.parentElement.clientWidth; // 使用父容器宽度
        const maxHeight = window.innerHeight * 0.7; // 使用视窗高度的70%

        // 计算保持比例的缩放系数
        const widthRatio = maxWidth / originalImage.width;
        const heightRatio = maxHeight / originalImage.height;
        const ratio = Math.min(widthRatio, heightRatio);

        // 设置画布尺寸
        imageCanvas.width = originalImage.width * ratio;
        imageCanvas.height = originalImage.height * ratio;
        scale = ratio;
    }
}

function updateButtonStates() {
    clearUserBoxes.disabled = (userBoxes.length === 0);
    clearServerBoxes.disabled = (serverBoxes.length === 0);
    clearAllBoxes.disabled = (allBoxes.length === 0);
    submitBoxes.disabled = (allBoxes.length === 0);
    deleteButton.disabled = (selectedBoxIndex === -1);
}

// 开始绘制
function startDrawing(e) {
    if (!drawingEnabled) return;
    
    e.preventDefault();
    isDrawing = true;

    // 清除当前选中状态
    selectedBoxIndex = -1;
    boxInfo.classList.add('hidden');
    updateButtonStates();
    
    const rect = imageCanvas.getBoundingClientRect();
    const mouseX = (e.clientX - rect.left) / scale;  // 转换为原始图像坐标
    const mouseY = (e.clientY - rect.top) / scale;
    
    startX = mouseX;
    startY = mouseY;
}

function handleCanvasMouseMove(e) {
    e.preventDefault();
    
    // 在绘制模式下始终保持十字光标，无论悬停在哪里
    if (drawingEnabled) {
        imageCanvas.style.cursor = 'crosshair';
        tooltip.style.opacity = '0'; // 始终隐藏工具提示
        
        if (isDrawing) {
            const rect = imageCanvas.getBoundingClientRect();
            const mouseX = (e.clientX - rect.left) / scale;
            const mouseY = (e.clientY - rect.top) / scale;
            
            currentBox = {
                x: Math.min(startX, mouseX),
                y: Math.min(startY, mouseY),
                width: Math.abs(mouseX - startX),
                height: Math.abs(mouseY - startY),
                source: 'user'
            };
            
            drawImage();
            
            // 绘制临时框（使用画布坐标）
            ctx.strokeStyle = USER_BOX_COLOR;
            ctx.lineWidth = 2;
            ctx.strokeRect(
                currentBox.x * scale,
                currentBox.y * scale,
                currentBox.width * scale,
                currentBox.height * scale
            );
        }
        return;
    }

    // 以下是非绘制状态的处理
    const rect = imageCanvas.getBoundingClientRect();
    const mouseX = (e.clientX - rect.left) / scale;
    const mouseY = (e.clientY - rect.top) / scale;
    
    // 默认隐藏工具提示和使用默认光标
    tooltip.style.opacity = '0';
    imageCanvas.style.cursor = 'default';

    // 检测悬停（使用原始图像坐标）
    let hoveredBox = null;
    for (let i = allBoxes.length - 1; i >= 0; i--) {
        const box = allBoxes[i];
        if (
            mouseX >= box.x &&
            mouseX <= box.x + box.width &&
            mouseY >= box.y &&
            mouseY <= box.y + box.height
        ) {
            hoveredBox = box;
            break;
        }
    }
    
    // 更新工具提示内容
    if (hoveredBox) {
        // 计算提示框位置
        const offset = 5;
        let left = e.pageX + offset;
        let top = e.pageY + offset;

        // 防止右侧溢出
        const tooltipWidth = tooltip.offsetWidth;
        if (left + tooltipWidth > window.innerWidth) {
            left = window.innerWidth - tooltipWidth - offset;
        }

        // 防止底部溢出
        const tooltipHeight = tooltip.offsetHeight;
        if (top + tooltipHeight > window.innerHeight) {
            top = window.innerHeight - tooltipHeight - offset;
        }

        tooltip.style.left = `${left}px`;
        tooltip.style.top = `${top}px`;
        
        // 添加成熟度等级信息（如果有）
        let maturityInfo = '';
        if (hoveredBox.maturityLevel !== undefined) {
            maturityInfo = `\n成熟度等级: ${hoveredBox.maturityLevel}`;
            if (hoveredBox.score !== undefined) {
                maturityInfo += `\n得分: ${hoveredBox.score}`;
            }
        }

        tooltip.textContent = `${hoveredBox.label} (${hoveredBox.source === 'server' ? '服务器' : '用户'})\n位置: (${Math.round(hoveredBox.x)}, ${Math.round(hoveredBox.y)})\n尺寸: ${Math.round(hoveredBox.width)}x${Math.round(hoveredBox.height)}${maturityInfo}`;
        tooltip.style.opacity = '1';
        imageCanvas.style.cursor = 'pointer';
    }
}

function endDrawing(e) {
    if (!isDrawing || !drawingEnabled) return;
    
    e.preventDefault();
    isDrawing = false;
    
    if (currentBox && currentBox.width > 5 && currentBox.height > 5) {
        currentBox.label = `用户框${userBoxes.length + 1}`;
        userBoxes.push(currentBox);
        allBoxes.push(currentBox);
        
        updateBoxCounts();
        clearUserBoxes.disabled = false;
        clearAllBoxes.disabled = false;
        submitBoxes.disabled = false;
        deleteButton.disabled = false;
        legendSection.classList.remove('hidden');
    }
    
    currentBox = null;
    drawImage();
}

function handleCanvasMouseOut(e) {
    e.preventDefault();
    isDrawing = false;
    tooltip.style.opacity = '0';
    
    // 绘制模式下，即使鼠标离开画布，也保持绘制状态
    if (drawingEnabled) {
        return;
    }
    
    imageCanvas.style.cursor = 'default';
}

// 画布点击处理
function handleCanvasClick(e) {
    // 在绘制模式下完全禁用点击选择
    if (drawingEnabled) {
        selectedBoxIndex = -1;
        boxInfo.classList.add('hidden');
        return;
    }
    
    const rect = imageCanvas.getBoundingClientRect();
    const clickX = (e.clientX - rect.left) / scale;  // 转换为原始图像坐标
    const clickY = (e.clientY - rect.top) / scale;
    
    let foundIndex = -1;
    for (let i = allBoxes.length - 1; i >= 0; i--) {
        const box = allBoxes[i];
        if (
            clickX >= box.x &&
            clickX <= box.x + box.width &&
            clickY >= box.y &&
            clickY <= box.y + box.height
        ) {
            foundIndex = i;
            break;
        }
    }    
    selectedBoxIndex = foundIndex;
    
    if (selectedBoxIndex !== -1) {
        const selectedBox = allBoxes[selectedBoxIndex];
        boxLabel.textContent = selectedBox.label;
        boxCoords.textContent = `(${Math.round(selectedBox.x)}, ${Math.round(selectedBox.y)})`;
        boxSize.textContent = `${Math.round(selectedBox.width)} x ${Math.round(selectedBox.height)}`;
        boxSource.textContent = selectedBox.source === 'server' ? '服务器检测' : '用户绘制';
        boxInfo.classList.remove('hidden');
        deleteButton.disabled = false;
    } else {
        boxInfo.classList.add('hidden');
        deleteButton.disabled = (userBoxes.length === 0 && serverBoxes.length === 0);
    }
    
    drawImage();
}

// 绘制图像
function drawImage() {
    if (!originalImage) return;
    
    ctx.clearRect(0, 0, imageCanvas.width, imageCanvas.height);
    
    // 绘制缩放后的图像
    ctx.drawImage(
        originalImage,
        0, 0, originalImage.width, originalImage.height,  // 源尺寸
        0, 0, imageCanvas.width, imageCanvas.height       // 目标尺寸
    );
    
    // 绘制所有框（转换为画布坐标）
    for (let i = 0; i < allBoxes.length; i++) {
        const box = allBoxes[i];
        
        // 计算画布坐标
        const x = box.x * scale;
        const y = box.y * scale;
        const width = box.width * scale;
        const height = box.height * scale;
        
        // 绘制框
        ctx.strokeStyle = box.source === 'server' ? SERVER_BOX_COLOR : USER_BOX_COLOR;
        if (i === selectedBoxIndex) ctx.strokeStyle = SELECTED_BOX_COLOR;
        ctx.lineWidth = 2;
        ctx.strokeRect(x, y, width, height);
        
        // 绘制标签
        ctx.fillStyle = ctx.strokeStyle;
        ctx.font = '12px Arial';
        ctx.fillText(box.label, x, y - 5);
        
        // 如果有成熟度等级，显示在框的右下角
        if (box.maturityLevel !== undefined) {
            ctx.fillStyle = box.source === 'server' ? SERVER_BOX_COLOR : USER_BOX_COLOR;
            if (i === selectedBoxIndex) ctx.fillStyle = SELECTED_BOX_COLOR;
            ctx.font = 'bold 14px Arial';
            ctx.fillText(`等级: ${box.maturityLevel}`, x + width - 60, y + height + 16);
            
            // 如果有分数，显示在成熟度下方
            if (box.score !== undefined) {
                ctx.fillText(`分数: ${box.score}`, x + width - 60, y + height + 34);
            }
        }
    }
}

// 上传图像
function uploadImage() {
    if (!originalImage) return;
    
    // 显示加载动画
    spinner.classList.remove('hidden');
    uploadButton.disabled = true;
    
    // 创建FormData
    const formData = new FormData();
    
    // 获取原始文件
    const file = imageInput.files[0];
    formData.append('image', file);
    
    // 发送请求
    fetch('/detect', {
        method: 'POST',
        body: formData
    })
    .then(response => {
        if (!response.ok) {
            throw new Error(`HTTP error! status: ${response.status}`);
        }
        return response.json();
    })
    .then(data => {
        // 隐藏加载动画
        spinner.classList.add('hidden');
        
        if (!data.detect_result || Object.keys(data.detect_result).length === 0) {
            showMessage('未检测到有效目标，请手动绘制', 'error');
            uploadButton.disabled = false;
            return;
        }
        
        // 清除现有框
        serverBoxes = [];
        allBoxes = [];
        
        // 存储成熟度等级数组
        currentMaturityLevels = data.predict_result || [];
        
        // 将检测结果转换为框结构
        let index = 0;
        for (const [key, box] of Object.entries(data.detect_result)) {
            const newBox = {
                x: box.x,
                y: box.y,
                width: box.width,
                height: box.height,
                label: key,
                source: 'server',
                maturityLevel: currentMaturityLevels[index] !== undefined ? currentMaturityLevels[index] : '未知'
            };
            
            // 添加到映射表以便后续查找
            detectionMapping[key] = index;
            
            serverBoxes.push(newBox);
            allBoxes.push(newBox);
            index++;
        }
        
        // 计算骨龄分数
        calculateScores();
        
        // 更新框计数
        updateBoxCounts();
        
        // 显示图例和结果
        legendSection.classList.remove('hidden');
        drawImage();
        
        // 显示骨龄结果
        calculateBoneAge();
        
        // 显示关节细节
        displayJointDetails();
        
        // 启用按钮
        drawButton.disabled = false;
        if (serverBoxes.length > 0) {
            clearServerBoxes.disabled = false;
            clearAllBoxes.disabled = false;
            submitBoxes.disabled = false;
        }
        
        uploadButton.disabled = false;
        
        showMessage('检测成功！已显示骨龄评估结果', 'success');
    })
    .catch(error => {
        console.error('Error:', error);
        spinner.classList.add('hidden');
        showMessage('上传失败，请稍后重试。', 'error');
        uploadButton.disabled = false;
    });
}

// 计算每个关节的成熟度分数
function calculateScores() {
    const gender = genderSelect.value;
    totalScore = 0;
    
    // 遍历所有服务器检测框
    for (let i = 0; i < serverBoxes.length; i++) {
        const box = serverBoxes[i];
        const boneKey = box.label; // 例如 'DIP1'
        const scoreKey = DETECTION_TO_SCORE_MAP[boneKey]; // 例如 'DIPFirst'
        
        if (scoreKey && SCORE[gender][scoreKey]) {
            const maturityLevel = box.maturityLevel;
            
            // 确保成熟度等级有效且在分数数组范围内
            if (maturityLevel !== undefined && maturityLevel > 0 && maturityLevel <= SCORE[gender][scoreKey].length) {
                // 根据成熟度等级获取分数（注意：成熟度等级从1开始，数组索引从0开始）
                const score = SCORE[gender][scoreKey][maturityLevel - 1];
                box.score = score;
                totalScore += score;
            }
        }
    }
    
    // 更新总分显示
    totalScoreValue.textContent = totalScore.toFixed(1);
}

// 计算骨龄
function calculateBoneAge() {
    const gender = genderSelect.value;
    let boneAge = 0;
    
    if (gender === 'boy') {
        // 男孩计算公式
        boneAge = 2.01790023656577 + (-0.0931820870747269) * totalScore + Math.pow(totalScore, 2) * 0.00334709095418796 +
        Math.pow(totalScore, 3) * (-3.32988302362153E-05) + Math.pow(totalScore, 4) * (1.75712910819776E-07) +
        Math.pow(totalScore, 5) * (-5.59998691223273E-10) + Math.pow(totalScore, 6) * (1.1296711294933E-12) +
        Math.pow(totalScore, 7) * (-1.45218037113138e-15) + Math.pow(totalScore, 8) * (1.15333377080353e-18) +
        Math.pow(totalScore, 9) * (-5.15887481551927e-22) + Math.pow(totalScore, 10) * (9.94098428102335e-26);
    } else if (gender === 'girl') {
        // 女孩计算公式
        boneAge = 5.81191794824917 + (-0.271546561737745) * totalScore +
        Math.pow(totalScore, 2) * 0.00526301486340724 + Math.pow(totalScore, 3) * (-4.37797717401925E-05) +
        Math.pow(totalScore, 4) * (2.0858722025667E-07) + Math.pow(totalScore, 5) * (-6.21879866563429E-10) +
        Math.pow(totalScore, 6) * (1.19909931745368E-12) + Math.pow(totalScore, 7) * (-1.49462900826936E-15) +
        Math.pow(totalScore, 8) * (1.162435538672E-18) + Math.pow(totalScore, 9) * (-5.12713017846218E-22) +
        Math.pow(totalScore, 10) * (9.78989966891478E-26);
    }
    
    // 四舍五入到两位小数
    boneAge = Math.round(boneAge * 100) / 100;
    
    // 更新显示
    boneAgeValue.textContent = boneAge.toFixed(2);
    boneAgeResult.classList.remove('hidden');
}

// 更新框计数
function updateBoxCounts() {
    serverBoxCount.textContent = serverBoxes.length;
    userBoxCount.textContent = userBoxes.length;
    boxCount.textContent = `总计: ${allBoxes.length}`;
}

// 切换绘制模式
function toggleDrawing() {
    drawingEnabled = !drawingEnabled;
    
    if (drawingEnabled) {
        // 进入绘制模式时清除选中状态
        selectedBoxIndex = -1;
        boxInfo.classList.add('hidden');
        updateButtonStates();
        
        // 进入绘制模式时隐藏工具提示
        tooltip.style.opacity = '0';

        drawButton.textContent = '停止绘制';
        imageCanvas.style.cursor = 'crosshair';
    } else {
        drawButton.textContent = '绘制目标框';
        imageCanvas.style.cursor = 'default';
    }
}

// 删除选中框
function deleteSelectedBox() {
    if (selectedBoxIndex === -1) return;
    
    const selectedBox = allBoxes[selectedBoxIndex];
    
    // 从相应数组中删除
    if (selectedBox.source === 'server') {
        serverBoxes = serverBoxes.filter(box => box !== selectedBox);
    } else {
        userBoxes = userBoxes.filter(box => box !== selectedBox);
    }
    
    // 从总数组中删除
    allBoxes.splice(selectedBoxIndex, 1);
    
    // 更新框计数
    updateBoxCounts();
    
    // 更新按钮状态
    if (serverBoxes.length === 0) {
        clearServerBoxes.disabled = true;
    }
    
    if (userBoxes.length === 0) {
        clearUserBoxes.disabled = true;
    }
    
    if (allBoxes.length === 0) {
        clearAllBoxes.disabled = true;
        submitBoxes.disabled = true;
        deleteButton.disabled = true;
        boxInfo.classList.add('hidden');
    }
    
    // 重置选中状态
    selectedBoxIndex = -1;
    boxInfo.classList.add('hidden');
    
    // 重新计算分数和骨龄
    calculateScores();
    calculateBoneAge();
    
    // 重绘
    drawImage();
}

// 清除框
function clearBoxes(type) {
    if (type === 'user') {
        userBoxes = [];
        clearUserBoxes.disabled = true;
    } else if (type === 'server') {
        serverBoxes = [];
        clearServerBoxes.disabled = true;
    } else if (type === 'all') {
        userBoxes = [];
        serverBoxes = [];
        clearUserBoxes.disabled = true;
        clearServerBoxes.disabled = true;
        clearAllBoxes.disabled = true;
    }
    
    // 更新总数组
    allBoxes = [...serverBoxes, ...userBoxes];
    
    // 更新框计数
    updateBoxCounts();
    
    // 更新按钮状态
    if (allBoxes.length === 0) {
        submitBoxes.disabled = true;
        deleteButton.disabled = true;
        legendSection.classList.add('hidden');
        boneAgeResult.classList.add('hidden');
        resultsContainer.classList.add('hidden');
    } else {
        // 重新计算分数和骨龄
        calculateScores();
        calculateBoneAge();
    }
    
    // 重置选中状态
    selectedBoxIndex = -1;
    boxInfo.classList.add('hidden');
    
    // 重绘
    drawImage();
}

// 提交所有框到服务器
function submitAllBoxes() {
    if (allBoxes.length === 0) return;
    
    // 显示加载动画
    spinner.classList.remove('hidden');
    submitBoxes.disabled = true;
    
    // 准备数据：所有框（服务器+用户）
    const boxData = {};
    
    // 转换框数据格式
    allBoxes.forEach(box => {
        boxData[box.label] = {
            x: Math.round(box.x),
            y: Math.round(box.y),
            width: Math.round(box.width),
            height: Math.round(box.height)
        };
    });
    
    // 发送请求
    fetch('/predict', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json'
        },
        body: JSON.stringify(boxData)
    })
    .then(response => {
        if (!response.ok) {
            throw new Error(`HTTP error! status: ${response.status}`);
        }
        return response.json();
    })
    .then(data => {
        // 隐藏加载动画
        spinner.classList.add('hidden');
        submitBoxes.disabled = false;
        
        if (data.success) {
            showMessage('框数据提交成功！', 'success');
            
            // 如果有返回的预测结果，更新成熟度等级
            if (data.predict_result && Array.isArray(data.predict_result)) {
                currentMaturityLevels = data.predict_result;
                
                // 更新框的成熟度等级
                let index = 0;
                for (const box of allBoxes) {
                    if (index < currentMaturityLevels.length) {
                        box.maturityLevel = currentMaturityLevels[index];
                        index++;
                    }
                }
                
                // 重新计算分数和骨龄
                calculateScores();
                calculateBoneAge();
                
                // 更新关节详情
                displayJointDetails();
                
                // 重绘
                drawImage();
            }
        } else {
            showMessage(data.message || '提交失败，请稍后重试。', 'error');
        }
    })
    .catch(error => {
        console.error('Error:', error);
        spinner.classList.add('hidden');
        submitBoxes.disabled = false;
        showMessage('提交失败，请稍后重试。', 'error');
    });
}

// 显示关节详情
function displayJointDetails() {
    // 清空现有内容
    jointsGrid.innerHTML = '';
    
    // 如果没有检测到关节，不显示详情区域
    if (serverBoxes.length === 0) {
        resultsContainer.classList.add('hidden');
        return;
    }
    
    // 创建每个关节的详情卡片
    for (const box of serverBoxes) {
        const jointCard = document.createElement('div');
        jointCard.className = 'joint-card';
        
        // 获取中文名称
        const chineseName = BONE_NAME_MAP[box.label] || box.label;
        
        // 创建卡片内容
        jointCard.innerHTML = `
            <div class="joint-name">${chineseName} (${box.label})</div>
            <div class="joint-details">
                <div class="joint-maturity">成熟度等级: <span class="maturity-value">${box.maturityLevel || '未知'}</span></div>
                <div class="joint-score">得分: <span class="score-value">${box.score !== undefined ? box.score : '未知'}</span></div>
            </div>
        `;
        
        jointsGrid.appendChild(jointCard);
    }
    
    // 显示结果容器
    resultsContainer.classList.remove('hidden');
}

// 当性别选择改变时重新计算骨龄
function recalculateBoneAge() {
    if (serverBoxes.length > 0) {
        calculateScores();
        calculateBoneAge();
        displayJointDetails();
    }
}

// 显示消息
function showMessage(text, type) {
    const message = document.createElement('div');
    message.className = `message ${type}`;
    message.textContent = text;
    
    messageArea.innerHTML = '';
    messageArea.appendChild(message);
    
    // 5秒后自动清除消息
    setTimeout(() => {
        message.style.opacity = '0';
        setTimeout(() => {
            if (messageArea.contains(message)) {
                messageArea.removeChild(message);
            }
        }, 300);
    }, 5000);
}

// 清除消息
function clearMessage() {
    messageArea.innerHTML = '';
}

// 初始化应用
init();