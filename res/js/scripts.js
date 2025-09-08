// 骨龄检测系统 - 现代化极简版本

// DOM 元素
const elements = {
    imageInput: document.getElementById('imageInput'),
    selectButton: document.getElementById('selectButton'),
    uploadButton: document.getElementById('uploadButton'),
    dropZone: document.getElementById('dropZone'),
    messageArea: document.getElementById('messageArea'),
    spinner: document.getElementById('spinner'),
    imageContainer: document.getElementById('imageContainer'),
    imageCanvas: document.getElementById('imageCanvas'),
    tooltip: document.getElementById('tooltip'),
    addBoxButton: document.getElementById('addBoxButton'),
    deleteBoxButton: document.getElementById('deleteBoxButton'),
    clearAllButton: document.getElementById('clearAllButton'),
    exportBoxesButton: document.getElementById('exportBoxesButton'),
    boxCount: document.getElementById('boxCount'),
    resultsPanel: document.getElementById('resultsPanel'),
    resultsGrid: document.getElementById('resultsGrid'),
    boxDetails: document.getElementById('boxDetails'),
    jointSelect: document.getElementById('jointSelect'),
    maturitySelect: document.getElementById('maturitySelect'),
    boxPosition: document.getElementById('boxPosition'),
    boxSize: document.getElementById('boxSize'),
    updateBoxButton: document.getElementById('updateBoxButton'),
    cancelEditButton: document.getElementById('cancelEditButton'),
    genderSelect: document.getElementById('genderSelect'),
    boneAgeResult: document.getElementById('boneAgeResult'),
    totalScore: document.getElementById('totalScore'),
    boneAge: document.getElementById('boneAge'),
    selectedGender: document.getElementById('selectedGender'),
    magnifier: document.getElementById('magnifier'),
    magnifierCanvas: document.getElementById('magnifierCanvas'),
    magnifierClose: document.getElementById('magnifierClose')
};

// 画布上下文
const ctx = elements.imageCanvas.getContext('2d');
const magnifierCtx = elements.magnifierCanvas.getContext('2d');

// 应用状态
const state = {
    image: null,
    imageFileName: '',
    boxes: [],
    selectedBoxIndex: -1,
    isDrawing: false,
    drawingMode: false,
    startPos: { x: 0, y: 0 },
    scale: 1,
    offset: { x: 0, y: 0 },
    drawingScheduled: false,
    lastDrawTime: 0,
    tempBox: null
};

// 常量
const CONFIG = {
    MAX_FILE_SIZE: 20 * 1024 * 1024, // 20MB
    BOX_COLORS: {
        normal: '#2563eb',
        selected: '#dc2626',
        hover: '#059669'
    },
    JOINT_NAMES: {
        radius: '桡骨',
        ulna: '尺骨',
        mcpfirst: '第一掌骨',
        mcpthird: '第三掌骨',
        mcpfifth: '第五掌骨',
        pipfirst: '第一近节指骨',
        pipthird: '第三近节指骨',
        pipfifth: '第五近节指骨',
        mipthird: '第三中节指骨',
        mipfifth: '第五中节指骨',
        dipfirst: '第一远节指骨',
        dipthird: '第三远节指骨',
        dipfifth: '第五远节指骨'
    },
    JOINT_NAMES_EN: {
        radius: 'Radius',
        ulna: 'Ulna',
        mcpfirst: 'First_Metacarpal',
        mcpthird: 'Third_Metacarpal',
        mcpfifth: 'Fifth_Metacarpal',
        pipfirst: 'First_Proximal_Phalanx',
        pipthird: 'Third_Proximal_Phalanx',
        pipfifth: 'Fifth_Proximal_Phalanx',
        mipthird: 'Third_Middle_Phalanx',
        mipfifth: 'Fifth_Middle_Phalanx',
        dipfirst: 'First_Distal_Phalanx',
        dipthird: 'Third_Distal_Phalanx',
        dipfifth: 'Fifth_Distal_Phalanx'
    },
    // 标准的13个骨骼列表
    REQUIRED_JOINTS: [
        'radius', 'ulna', 'mcpfirst', 'mcpthird', 'mcpfifth',
        'pipfirst', 'pipthird', 'pipfifth', 'mipthird', 'mipfifth',
        'dipfirst', 'dipthird', 'dipfifth'
    ]
};

// 骨龄评分系统
const SCORE = {
    'girl': {
        'radius': [0, 10, 15, 22, 25, 40, 59, 91, 125, 138, 178, 192, 199, 203, 210],
        'ulna': [0, 27, 31, 36, 50, 73, 95, 120, 157, 168, 176, 182, 189],
        'mcpfirst': [0, 5, 7, 10, 16, 23, 28, 34, 41, 47, 53, 66],
        'mcpthird': [0, 3, 5, 6, 9, 14, 21, 32, 40, 47, 51],
        'mcpfifth': [0, 4, 5, 7, 10, 15, 22, 33, 43, 47, 51],
        'pipfirst': [0, 6, 7, 8, 11, 17, 26, 32, 38, 45, 53, 60, 67],
        'pipthird': [0, 3, 5, 7, 9, 15, 20, 25, 29, 35, 41, 46, 51],
        'pipfifth': [0, 4, 5, 7, 11, 18, 21, 25, 29, 34, 40, 45, 50],
        'mipthird': [0, 4, 5, 7, 10, 16, 21, 25, 29, 35, 43, 46, 51],
        'mipfifth': [0, 3, 5, 7, 12, 19, 23, 27, 32, 35, 39, 43, 49],
        'dipfirst': [0, 5, 6, 8, 10, 20, 31, 38, 44, 45, 52, 67],
        'dipthird': [0, 3, 5, 7, 10, 16, 24, 30, 33, 36, 39, 49],
        'dipfifth': [0, 5, 6, 7, 11, 18, 25, 29, 33, 35, 39, 49]
    },
    'boy': {
        'radius': [0, 8, 11, 15, 18, 31, 46, 76, 118, 135, 171, 188, 197, 201, 209],
        'ulna': [0, 25, 30, 35, 43, 61, 80, 116, 157, 168, 180, 187, 194],
        'mcpfirst': [0, 4, 5, 8, 16, 22, 26, 34, 39, 45, 52, 66],
        'mcpthird': [0, 3, 4, 5, 8, 13, 19, 30, 38, 44, 51],
        'mcpfifth': [0, 3, 4, 6, 9, 14, 19, 31, 41, 46, 50],
        'pipfirst': [0, 4, 5, 7, 11, 17, 23, 29, 36, 44, 52, 59, 66],
        'pipthird': [0, 3, 4, 5, 8, 14, 19, 23, 28, 34, 40, 45, 50],
        'pipfifth': [0, 3, 4, 6, 10, 16, 19, 24, 28, 33, 40, 44, 50],
        'mipthird': [0, 3, 4, 5, 9, 14, 18, 23, 28, 35, 42, 45, 50],
        'mipfifth': [0, 3, 4, 6, 11, 17, 21, 26, 31, 36, 40, 43, 49],
        'dipfirst': [0, 4, 5, 6, 9, 19, 28, 36, 43, 46, 51, 67],
        'dipthird': [0, 3, 4, 5, 9, 15, 23, 29, 33, 37, 40, 49],
        'dipfifth': [0, 3, 4, 6, 11, 17, 23, 29, 32, 36, 40, 49]
    }
};

// 骨龄计算函数
function calculateBoneAge(gender, maturityLevels) {
    let totalScore = 0;
    
    // 计算总分
    for (const [jointName, level] of Object.entries(maturityLevels)) {
        if (SCORE[gender] && SCORE[gender][jointName]) {
            const scores = SCORE[gender][jointName];
            // 等级从0开始，直接映射分值数组
            const scoreIndex = level;
            if (scoreIndex >= 0 && scoreIndex < scores.length) {
                totalScore += scores[scoreIndex];
            }
        }
    }
    
    let boneAge = 0;
    
    if (gender === 'boy') {
        // 男孩计算公式
        boneAge = 2.01790023656577 + (-0.0931820870747269) * totalScore + 
            Math.pow(totalScore, 2) * 0.00334709095418796 + 
            Math.pow(totalScore, 3) * (-3.32988302362153E-05) + 
            Math.pow(totalScore, 4) * (1.75712910819776E-07) + 
            Math.pow(totalScore, 5) * (-5.59998691223273E-10) + 
            Math.pow(totalScore, 6) * (1.1296711294933E-12) + 
            Math.pow(totalScore, 7) * (-1.45218037113138e-15) + 
            Math.pow(totalScore, 8) * (1.15333377080353e-18) + 
            Math.pow(totalScore, 9) * (-5.15887481551927e-22) + 
            Math.pow(totalScore, 10) * (9.94098428102335e-26);
    } else if (gender === 'girl') {
        // 女孩计算公式
        boneAge = 5.81191794824917 + (-0.271546561737745) * totalScore + 
            Math.pow(totalScore, 2) * 0.00526301486340724 + 
            Math.pow(totalScore, 3) * (-4.37797717401925E-05) + 
            Math.pow(totalScore, 4) * (2.0858722025667E-07) + 
            Math.pow(totalScore, 5) * (-6.21879866563429E-10) + 
            Math.pow(totalScore, 6) * (1.19909931745368E-12) + 
            Math.pow(totalScore, 7) * (-1.49462900826936E-15) + 
            Math.pow(totalScore, 8) * (1.162435538672E-18) + 
            Math.pow(totalScore, 9) * (-5.12713017846218E-22) + 
            Math.pow(totalScore, 10) * (9.78989966891478E-26);
    }
    
    return {
        totalScore: totalScore,
        boneAge: Math.max(0, boneAge) // 确保骨龄不为负数
    };
}

// 初始化应用
function init() {
    bindEvents();
    updateUI();
}

// 绑定事件监听器
function bindEvents() {
    // 文件选择和上传
    elements.selectButton.addEventListener('click', () => elements.imageInput.click());
    elements.imageInput.addEventListener('change', handleFileSelect);
    elements.uploadButton.addEventListener('click', uploadImage);
    
    // 拖拽上传
    elements.dropZone.addEventListener('dragover', handleDragOver);
    elements.dropZone.addEventListener('dragleave', handleDragLeave);
    elements.dropZone.addEventListener('drop', handleDrop);
    
    // 工具栏按钮
    elements.addBoxButton.addEventListener('click', toggleDrawingMode);
    elements.deleteBoxButton.addEventListener('click', deleteSelectedBox);
    elements.clearAllButton.addEventListener('click', clearAllBoxes);
    elements.exportBoxesButton.addEventListener('click', exportAnnotatedRegions);
    
    // 画布交互
    elements.imageCanvas.addEventListener('mousedown', handleCanvasMouseDown);
    elements.imageCanvas.addEventListener('mousemove', handleCanvasMouseMove);
    elements.imageCanvas.addEventListener('mouseup', handleCanvasMouseUp);
    elements.imageCanvas.addEventListener('click', handleCanvasClick);
    elements.imageCanvas.addEventListener('mouseleave', hideTooltip);
    
    // 框详情编辑
    elements.updateBoxButton.addEventListener('click', updateSelectedBox);
    elements.cancelEditButton.addEventListener('click', cancelEdit);
    
    // 关节类型改变时更新成熟度选项
    elements.jointSelect.addEventListener('change', (e) => {
        updateMaturityOptions(e.target.value);
    });
    
    // 性别改变时更新成熟度选项和重新计算骨龄
    elements.genderSelect.addEventListener('change', () => {
        if (state.selectedBoxIndex >= 0) {
            const box = state.boxes[state.selectedBoxIndex];
            updateMaturityOptions(box.joint);
        }
        // 如果有检测结果，重新显示结果（包括重新计算骨龄和更新卡片得分）
        if (state.boxes.length > 0 && elements.resultsPanel.style.display !== 'none') {
            displayResults();
        }
    });
    
    // 窗口大小变化
    window.addEventListener('resize', debounce(handleResize, 250));
    elements.magnifierClose.addEventListener('click', hideMagnifier);
}

// 文件选择处理
function handleFileSelect(event) {
    const file = event.target.files[0];
    if (file) {
        validateAndLoadImage(file);
    }
}

// 拖拽处理
function handleDragOver(event) {
    event.preventDefault();
    elements.dropZone.classList.add('dragover');
}

function handleDragLeave(event) {
    event.preventDefault();
    elements.dropZone.classList.remove('dragover');
}

function handleDrop(event) {
    event.preventDefault();
    elements.dropZone.classList.remove('dragover');
    
    const files = event.dataTransfer.files;
    if (files.length > 0) {
        validateAndLoadImage(files[0]);
    }
}

// 验证并加载图片
function validateAndLoadImage(file) {
    // 文件类型检查
    if (!file.type.startsWith('image/')) {
        showMessage('请选择有效的图片文件', 'error');
        return;
    }
    
    // 文件大小检查
    if (file.size > CONFIG.MAX_FILE_SIZE) {
        showMessage('文件大小超过限制（20MB）', 'error');
        return;
    }

    state.imageFileName = file.name;
    
    // 清除之前的坐标框、消息和结果
    clearAllBoxes();
    clearMessages();
    elements.resultsPanel.style.display = 'none';
    elements.boneAgeResult.style.display = 'none';
    
    // 加载图片
    const reader = new FileReader();
    reader.onload = (e) => {
        const img = new Image();
        img.onload = () => {
            state.image = img;
            setupCanvas();
            drawCanvas();
            elements.uploadButton.disabled = false;
            showMessage('图片加载成功，点击上传进行检测', 'success');
        };
        img.src = e.target.result;
    };
    reader.readAsDataURL(file);
}

// 设置画布
function setupCanvas() {
    if (!state.image) return;
    
    // 关键修复：先显示容器，再获取其尺寸
    elements.imageContainer.style.display = 'flex';

    const container = elements.imageContainer.querySelector('.image-wrapper'); // 获取真正的容器
    const maxWidth = container.clientWidth;  // 直接使用 wrapper 的宽度
    const maxHeight = container.clientHeight; // 直接使用 wrapper 的高度
    
    // 计算缩放比例
    const scaleX = maxWidth / state.image.width;
    const scaleY = maxHeight / state.image.height;
    state.scale = Math.min(scaleX, scaleY, 1); // 不放大，只缩小
    
    // 计算显示尺寸
    const displayWidth = state.image.width * state.scale;
    const displayHeight = state.image.height * state.scale;
    
    // 设置画布的逻辑尺寸
    elements.imageCanvas.width = displayWidth;
    elements.imageCanvas.height = displayHeight;
    
    // 通过CSS控制显示尺寸（这部分可以省略，因为居中已由CSS处理）
    elements.imageCanvas.style.width = displayWidth + 'px';
    elements.imageCanvas.style.height = displayHeight + 'px';
    
    // 偏移量现在由CSS处理，JS中不再需要
    state.offset.x = 0;
    state.offset.y = 0;
}

// 绘制画布
function drawCanvas() {
    if (!state.image) return;
    
    if (state.drawingScheduled) return;
    state.drawingScheduled = true;
    
    requestAnimationFrame(() => {
        ctx.clearRect(0, 0, elements.imageCanvas.width, elements.imageCanvas.height);
        
        const scaledWidth = state.image.width * state.scale;
        const scaledHeight = state.image.height * state.scale;
        ctx.drawImage(state.image, 0, 0, scaledWidth, scaledHeight);
        
        state.boxes.forEach((box, index) => {
            drawBox(box, index === state.selectedBoxIndex);
        });
        
        if (state.isDrawing && state.tempBox) {
            const { x, y, width, height } = state.tempBox;
            const displayX = x * state.scale;
            const displayY = y * state.scale;
            const displayWidth = width * state.scale;
            const displayHeight = height * state.scale;

            ctx.strokeStyle = CONFIG.BOX_COLORS.normal;
            ctx.lineWidth = 2;
            ctx.setLineDash([5, 5]);
            ctx.strokeRect(displayX, displayY, displayWidth, displayHeight);
            ctx.setLineDash([]);
        }
        
        state.drawingScheduled = false;
    });
}

// 绘制单个框
function drawBox(box, isSelected = false) {
    // 将原始坐标转换为显示坐标
    const x = box.box.x * state.scale;
    const y = box.box.y * state.scale;
    const width = box.box.width * state.scale;
    const height = box.box.height * state.scale;
    
    // 设置固定的线宽，不受缩放影响
    ctx.strokeStyle = isSelected ? CONFIG.BOX_COLORS.selected : CONFIG.BOX_COLORS.normal;
    ctx.lineWidth = isSelected ? 3 : 2;
    ctx.strokeRect(x, y, width, height);
    
    // 绘制标签，使用固定字体大小
    const label = `${CONFIG.JOINT_NAMES[box.joint] || box.joint} (${box.maturity_stage}级)`;
    const labelY = y > 25 ? y - 5 : y + height + 20;
    
    // 设置固定字体大小
    ctx.font = '14px sans-serif';
    const textMetrics = ctx.measureText(label);
    
    ctx.fillStyle = 'rgba(0, 0, 0, 0.7)';
    ctx.fillRect(x, labelY - 18, textMetrics.width + 8, 20);
    
    ctx.fillStyle = 'white';
    ctx.fillText(label, x + 4, labelY - 4);
}

// 验证检测结果是否包含正确的13个骨骼
function validateDetectionResults(boxes) {
    const detectedJoints = boxes.map(box => box.joint);
    const missingJoints = [];
    const duplicateJoints = [];
    const invalidJoints = [];
    
    // 检查缺失的骨骼
    CONFIG.REQUIRED_JOINTS.forEach(joint => {
        if (!detectedJoints.includes(joint)) {
            missingJoints.push(joint);
        }
    });
    
    // 检查重复的骨骼
    const jointCounts = {};
    detectedJoints.forEach(joint => {
        jointCounts[joint] = (jointCounts[joint] || 0) + 1;
        if (jointCounts[joint] > 1) {
            duplicateJoints.push(joint);
        }
    });
    
    // 检查无效的骨骼（不在标准列表中）
    detectedJoints.forEach(joint => {
        if (!CONFIG.REQUIRED_JOINTS.includes(joint)) {
            invalidJoints.push(joint);
        }
    });
    
    const isValid = missingJoints.length === 0 && duplicateJoints.length === 0 && invalidJoints.length === 0;
    
    return {
        isValid,
        missingJoints,
        duplicateJoints,
        invalidJoints,
        detectedCount: detectedJoints.length,
        expectedCount: CONFIG.REQUIRED_JOINTS.length
    };
}

// 生成验证错误消息
function generateValidationMessage(validation) {
    if (validation.isValid) {
        return '检测结果验证通过，包含完整的13个骨骼';
    }
    
    let messages = [];
    
    if (validation.missingJoints.length > 0) {
        const missingNames = validation.missingJoints.map(joint => CONFIG.JOINT_NAMES[joint] || joint);
        messages.push(`缺失骨骼: ${missingNames.join('、')}`);
    }
    
    if (validation.duplicateJoints.length > 0) {
        const duplicateNames = [...new Set(validation.duplicateJoints)].map(joint => CONFIG.JOINT_NAMES[joint] || joint);
        messages.push(`重复检测: ${duplicateNames.join('、')}`);
    }
    
    if (validation.invalidJoints.length > 0) {
        const invalidNames = [...new Set(validation.invalidJoints)].map(joint => CONFIG.JOINT_NAMES[joint] || joint);
        messages.push(`无效骨骼: ${invalidNames.join('、')}`);
    }
    
    messages.push(`检测到${validation.detectedCount}个骨骼，期望${validation.expectedCount}个`);
    
    return messages.join('；');
}

// 上传图片进行检测
async function uploadImage() {
    if (!state.image) return;
    
    const formData = new FormData();
    formData.append('image', elements.imageInput.files[0]);
    
    try {
        showSpinner(true);
        elements.uploadButton.disabled = true;
        
        const response = await fetch('/predict', {
            method: 'POST',
            body: formData
        });
        
        if (!response.ok) {
            throw new Error(`HTTP ${response.status}: ${response.statusText}`);
        }
        
        const result = await response.json();
        
        if (result.is_valid && result.bones_detail) {
            // 验证检测结果
            const validation = validateDetectionResults(result.bones_detail);
            const validationMessage = generateValidationMessage(validation);
            
            // 无论验证是否通过，都显示检测结果
            // 为每个box添加原始等级信息
            state.boxes = result.bones_detail.map(box => ({
                ...box,
                original_maturity_stage: box.maturity_stage
            }));
            drawCanvas();
            displayResults();
            updateUI();
            
            if (validation.isValid) {
                showMessage(`检测完成！${validationMessage}`, 'success');
            } else {
                showMessage(`检测完成，但存在问题：${validationMessage}`, 'warning');
            }
        } else if (result.bones_detail && result.bones_detail.length > 0) {
            // 即使is_valid为false，如果有检测结果也要显示
            const validation = validateDetectionResults(result.bones_detail);
            const validationMessage = generateValidationMessage(validation);
            
            // 为每个box添加原始等级信息
            state.boxes = result.bones_detail.map(box => ({
                ...box,
                original_maturity_stage: box.maturity_stage
            }));
            drawCanvas();
            displayResults();
            updateUI();
            
            showMessage(`检测完成，但存在问题：${validationMessage}`, 'warning');
        } else {
            showMessage('检测失败，未检测到任何骨骼，请重试', 'error');
        }
        
    } catch (error) {
        console.error('Upload error:', error);
        showMessage(`检测失败: ${error.message}`, 'error');
    } finally {
        showSpinner(false);
        elements.uploadButton.disabled = false;
    }
}

// 显示检测结果
function displayResults() {
    elements.resultsGrid.innerHTML = '';
    
    state.boxes.forEach((box, index) => {
        const card = createResultCard(box, index);
        elements.resultsGrid.appendChild(card);
    });
    
    // 计算并显示骨龄
    calculateAndDisplayBoneAge();
    
    elements.resultsPanel.style.display = 'block';
}

// 计算并显示骨龄
function calculateAndDisplayBoneAge() {
    if (state.boxes.length === 0) {
        elements.boneAgeResult.style.display = 'none';
        return;
    }
    
    const gender = elements.genderSelect.value;
    const maturityLevels = {};
    
    // 收集所有骨骼的成熟度等级
    state.boxes.forEach(box => {
        if (box.joint && box.maturity_stage !== undefined) {
            maturityLevels[box.joint] = parseInt(box.maturity_stage);
        }
    });
    
    // 计算骨龄
    const result = calculateBoneAge(gender, maturityLevels);
    
    // 显示结果
    elements.totalScore.textContent = result.totalScore;
    elements.boneAge.textContent = result.boneAge.toFixed(1);
    elements.selectedGender.textContent = gender === 'boy' ? '男孩' : '女孩';
    elements.boneAgeResult.style.display = 'block';
}

// 创建结果卡片
function createResultCard(box, index) {
    const card = document.createElement('div');
    card.className = 'result-card';
    
    // 构建成熟度显示内容
    let maturityDisplay;
    if (box.original_maturity_stage && box.original_maturity_stage !== box.maturity_stage) {
        // 显示服务器等级 -> 用户修改等级
        maturityDisplay = `<span class="maturity-badge original">${box.original_maturity_stage}级</span> → <span class="maturity-badge modified">${box.maturity_stage}级</span>`;
    } else {
        // 只显示当前等级
        maturityDisplay = `<span class="maturity-badge">${box.maturity_stage}级</span>`;
    }
    
    // 计算当前关节的得分
    let scoreDisplay = '未知';
    const currentGender = elements.genderSelect.value;
    if (SCORE[currentGender] && SCORE[currentGender][box.joint]) {
        const scores = SCORE[currentGender][box.joint];
        const scoreIndex = box.maturity_stage; // 等级从0开始，直接映射数组
        if (scoreIndex >= 0 && scoreIndex < scores.length) {
            scoreDisplay = scores[scoreIndex];
        }
    }
    
    card.innerHTML = `
        <h5>${CONFIG.JOINT_NAMES[box.joint] || box.joint}</h5>
        <div class="detail">位置: (${box.box.x}, ${box.box.y})</div>
        <div class="detail">尺寸: ${box.box.width} × ${box.box.height}</div>
        <div class="detail">成熟度: ${maturityDisplay}</div>
        <div class="detail">得分: <span class="score-value">${scoreDisplay}</span></div>
    `;
    
    card.addEventListener('click', () => selectBox(index));
    return card;
}

// 画布鼠标事件处理
function handleCanvasMouseDown(event) {
    if (!state.drawingMode) return;
    
    const rect = elements.imageCanvas.getBoundingClientRect();
    // 转换为原始图像坐标（整数）
    state.startPos = {
        x: Math.round((event.clientX - rect.left) / state.scale),
        y: Math.round((event.clientY - rect.top) / state.scale)
    };
    state.isDrawing = true;
}

function handleCanvasMouseMove(event) {
    if (!state.isDrawing || !state.drawingMode) return;
    
    const rect = elements.imageCanvas.getBoundingClientRect();
    const currentX = Math.round((event.clientX - rect.left) / state.scale);
    const currentY = Math.round((event.clientY - rect.top) / state.scale);
    
    // 更新临时框的状态
    state.tempBox = {
        x: Math.min(state.startPos.x, currentX),
        y: Math.min(state.startPos.y, currentY),
        width: Math.abs(currentX - state.startPos.x),
        height: Math.abs(currentY - state.startPos.y)
    };
    
    drawCanvas(); 
}

function handleCanvasMouseUp(event) {
    if (!state.isDrawing || !state.drawingMode) return;
    
    // 结束绘制前，先清除临时框状态
    state.isDrawing = false;
    const finalBox = state.tempBox;
    state.tempBox = null;

    // 只有当框足够大时才创建
    if (finalBox && finalBox.width > 10 && finalBox.height > 10) {
        const newBox = {
            box: {
                x: finalBox.x,
                y: finalBox.y,
                width: finalBox.width,
                height: finalBox.height
            },
            category_id: 0,
            joint: 'radius',
            maturity_stage: 0
        };
        
        state.boxes.push(newBox);
        selectBox(state.boxes.length - 1);
        displayResults();
        updateUI();
    }
    
    drawCanvas(); // 最后重绘一次，清除可能残留的临时框
    toggleDrawingMode(); // 退出绘制模式
}

function handleCanvasClick(event) {
    if (state.drawingMode) return;
    
    const rect = elements.imageCanvas.getBoundingClientRect();
    // 转换为原始图像坐标
    const x = (event.clientX - rect.left) / state.scale;
    const y = (event.clientY - rect.top) / state.scale;
    
    const clickedBox = findBoxAtPosition(x, y);
    selectBox(clickedBox);
}

// 查找指定位置的框
function findBoxAtPosition(x, y) {
    for (let i = state.boxes.length - 1; i >= 0; i--) {
        const box = state.boxes[i].box;
        if (x >= box.x && x <= box.x + box.width &&
            y >= box.y && y <= box.y + box.height) {
            return i;
        }
    }
    return -1;
}

// 选择框
function selectBox(index) {
    state.selectedBoxIndex = index;
    
    // 更新结果卡片样式
    document.querySelectorAll('.result-card').forEach((card, i) => {
        card.classList.toggle('selected', i === index);
    });
    
    if (index >= 0) {
        showBoxDetails(state.boxes[index]);
        showMagnifier(state.boxes[index]);
    } else {
        hideBoxDetails();
        hideMagnifier();
    }
    
    drawCanvas();
    updateUI();
}

// 更新成熟度选项
function updateMaturityOptions(jointType) {
    const gender = elements.genderSelect.value;
    const scores = SCORE[gender] && SCORE[gender][jointType];
    
    // 清空现有选项
    elements.maturitySelect.innerHTML = '';
    
    if (scores) {
        // 根据评分数组长度生成选项，等级从0开始
        for (let i = 0; i < scores.length; i++) {
            const option = document.createElement('option');
            option.value = i; // 等级从0开始
            option.textContent = `等级 ${i}`;
            elements.maturitySelect.appendChild(option);
        }
    }
}

// 显示框详情
function showBoxDetails(box) {
    elements.jointSelect.value = box.joint;
    updateMaturityOptions(box.joint);
    elements.maturitySelect.value = box.maturity_stage;
    elements.boxPosition.textContent = `(${box.box.x}, ${box.box.y})`;
    elements.boxSize.textContent = `${box.box.width} × ${box.box.height}`;
    elements.boxDetails.style.display = 'block';
}

// 隐藏框详情
function hideBoxDetails() {
    elements.boxDetails.style.display = 'none';
}

// 更新选中的框
function updateSelectedBox() {
    if (state.selectedBoxIndex >= 0) {
        const box = state.boxes[state.selectedBoxIndex];
        
        // 如果还没有原始等级信息，保存当前值作为原始值
        if (!box.original_maturity_stage) {
            box.original_maturity_stage = box.maturity_stage;
        }
        
        box.joint = elements.jointSelect.value;
        box.maturity_stage = parseInt(elements.maturitySelect.value);
        
        drawCanvas();
        displayResults();
        showMessage('框信息已更新', 'success');
    }
}

// 取消编辑
function cancelEdit() {
    selectBox(-1);
}

// 切换绘制模式
function toggleDrawingMode() {
    state.drawingMode = !state.drawingMode;
    elements.addBoxButton.textContent = state.drawingMode ? '取消绘制' : '添加标注框';
    elements.imageCanvas.style.cursor = state.drawingMode ? 'crosshair' : 'default';
    
    if (!state.drawingMode) {
        selectBox(-1);
    }
}

// 删除选中的框
function deleteSelectedBox() {
    if (state.selectedBoxIndex >= 0) {
        state.boxes.splice(state.selectedBoxIndex, 1);
        selectBox(-1);
        drawCanvas();
        displayResults();
        updateUI();
        showMessage('框已删除', 'success');
    }
}

// 清除所有框
function clearAllBoxes() {
    if (state.boxes.length > 0 && confirm('确定要清除所有标注框吗？')) {
        state.boxes = [];
        selectBox(-1);
        drawCanvas();
        elements.resultsPanel.style.display = 'none';
        updateUI();
        showMessage('所有框已清除', 'success');
    }
}

// 导出标注区域
async function exportAnnotatedRegions() {
    if (!state.image || state.boxes.length === 0) {
        showMessage('没有可导出的标注区域', 'warning');
        return;
    }
    
    showSpinner(true);
    showMessage('正在生成ZIP文件，请稍候...', 'info');

    try {
        // 初始化 JSZip
        const zip = new JSZip();
        
        // 创建一个临时画布用于裁剪
        const tempCanvas = document.createElement('canvas');
        const tempCtx = tempCanvas.getContext('2d');
        
        const validBoxes = state.boxes.filter(box => box.joint && box.maturity_stage !== undefined);
        
        if (validBoxes.length === 0) {
            showMessage('没有有效的标注区域可导出', 'warning');
            showSpinner(false);
            return;
        }

        // 异步处理每一个标注框
        for (const box of validBoxes) {
            const { x, y, width, height } = box.box;
            
            // 设置临时画布尺寸并进行裁剪
            tempCanvas.width = width;
            tempCanvas.height = height;
            tempCtx.drawImage(state.image, x, y, width, height, 0, 0, width, height);
            
            // 将画布内容转换为 Blob 对象
            const blob = await new Promise(resolve => tempCanvas.toBlob(resolve, 'image/png'));
            
            if (blob) {
                // 生成英文文件名：关节名_Grade等级.png
                const jointNameEn = CONFIG.JOINT_NAMES_EN[box.joint] || box.joint;
                const fileName = `${jointNameEn}_Grade${box.maturity_stage}.png`;
                
                // 将图像 Blob 添加到 zip 对象中
                zip.file(fileName, blob);
            }
        }
        
        // 生成 ZIP 文件的 Blob
        const zipBlob = await zip.generateAsync({
            type: 'blob',
            compression: 'DEFLATE',
            compressionOptions: {
                level: 9 // 0-9 的压缩等级，9为最高
            }
        });

        const baseFileName = state.imageFileName.split('.').slice(0, -1).join('.') || 'annotations';
        
        // 2. 构造新的下载文件名
        const downloadFileName = `${baseFileName}_annotations.zip`;

        // 创建一个隐藏的下载链接并触发点击
        const link = document.createElement('a');
        link.href = URL.createObjectURL(zipBlob);
        link.download = downloadFileName;
        document.body.appendChild(link);
        link.click();
        document.body.removeChild(link);
        
        // 释放 Blob URL 占用的内存
        URL.revokeObjectURL(link.href);

        showMessage(`成功导出 ${validBoxes.length} 个标注到ZIP文件`, 'success');

    } catch (error) {
        console.error('ZIP Export error:', error);
        showMessage(`导出失败: ${error.message}`, 'error');
    } finally {
        showSpinner(false);
        // 清除“正在生成”的消息
        clearMessages();
    }
}

// 备用的默认下载方式
function exportWithDefaultDownload() {
    if (!state.image || state.boxes.length === 0) {
        return;
    }
    
    try {
        // 创建一个临时画布用于裁剪
        const tempCanvas = document.createElement('canvas');
        const tempCtx = tempCanvas.getContext('2d');
        
        let exportCount = 0;
        
        state.boxes.forEach((box, index) => {
            if (!box.joint || box.maturity_stage === undefined) {
                return; // 跳过没有完整信息的框
            }
            
            // 确保坐标为整数并在图像范围内
            const x = Math.max(0, Math.floor(box.box.x));
            const y = Math.max(0, Math.floor(box.box.y));
            const width = Math.min(Math.floor(box.box.width), state.image.width - x);
            const height = Math.min(Math.floor(box.box.height), state.image.height - y);
            
            if (width <= 0 || height <= 0) {
                console.warn(`跳过无效的框: x=${x}, y=${y}, width=${width}, height=${height}`);
                return;
            }
            
            // 设置临时画布尺寸
            tempCanvas.width = width;
            tempCanvas.height = height;
            
            // 清除画布
            tempCtx.clearRect(0, 0, width, height);
            
            // 裁剪图像区域
            tempCtx.drawImage(
                state.image,
                x, y, width, height,  // 源区域
                0, 0, width, height   // 目标区域
            );
            
            // 生成英文文件名：关节英文名称 + 等级
            const jointNameEn = CONFIG.JOINT_NAMES_EN[box.joint] || box.joint;
            const fileName = `${jointNameEn}_Grade${box.maturity_stage}.png`;
            
            // 转换为blob并下载
            tempCanvas.toBlob((blob) => {
                if (blob) {
                    const url = URL.createObjectURL(blob);
                    const link = document.createElement('a');
                    link.href = url;
                    link.download = fileName;
                    document.body.appendChild(link);
                    link.click();
                    document.body.removeChild(link);
                    URL.revokeObjectURL(url);
                    exportCount++;
                    
                    // 如果是最后一个，显示完成消息
                    if (exportCount === state.boxes.filter(b => b.joint && b.maturity_stage !== undefined).length) {
                        showMessage(`成功导出 ${exportCount} 个标注区域`, 'success');
                    }
                }
            }, 'image/png');
        });
        
        if (exportCount === 0) {
            showMessage('没有有效的标注区域可导出', 'warning');
        }
        
    } catch (error) {
        console.error('Export error:', error);
        showMessage('导出失败，请重试', 'error');
    }
}

// 显示工具提示
function showTooltip(event, box) {
    const tooltip = elements.tooltip;
    const jointName = CONFIG.JOINT_NAMES[box.joint] || box.joint;
    tooltip.innerHTML = `${jointName}<br>成熟度: ${box.maturity_stage}级`;
    tooltip.style.left = event.pageX + 10 + 'px';
    tooltip.style.top = event.pageY - 10 + 'px';
    tooltip.classList.add('show');
}

// 隐藏工具提示
function hideTooltip() {
    elements.tooltip.classList.remove('show');
}

// 显示消息
function showMessage(text, type = 'info') {
    const message = document.createElement('div');
    message.className = `message ${type}`;
    message.textContent = text;
    
    elements.messageArea.appendChild(message);
    
    // 只有非错误信息才自动移除
    if (type !== 'error') {
        setTimeout(() => {
            if (message.parentNode) {
                message.parentNode.removeChild(message);
            }
        }, 3000);
    }
}

// 清除所有消息
function clearMessages() {
    elements.messageArea.innerHTML = '';
}

// 显示/隐藏加载动画
function showSpinner(show) {
    elements.spinner.classList.toggle('hidden', !show);
}

// 更新UI状态
function updateUI() {
    const hasBoxes = state.boxes.length > 0;
    const hasSelection = state.selectedBoxIndex >= 0;
    const hasImage = state.image !== null;
    
    elements.deleteBoxButton.disabled = !hasSelection;
    elements.clearAllButton.disabled = !hasBoxes;
    elements.exportBoxesButton.disabled = !hasBoxes || !hasImage;
    elements.boxCount.textContent = state.boxes.length;
}

// 窗口大小变化处理
function handleResize() {
    if (state.image) {
        setupCanvas();
        drawCanvas();
    }
}

// 防抖函数
function debounce(func, wait) {
    let timeout;
    return function executedFunction(...args) {
        const later = () => {
            clearTimeout(timeout);
            func(...args);
        };
        clearTimeout(timeout);
        timeout = setTimeout(later, wait);
    };
}

function showMagnifier(box) {
    if (!state.image || !box) return;
    
    elements.magnifier.classList.add('show');
    drawMagnifierContent(box);
}

function hideMagnifier() {
    elements.magnifier.classList.remove('show');
}

function drawMagnifierContent(box) {
    const canvas = elements.magnifierCanvas;
    const ctx = magnifierCtx;
    
    if (!state.image) {
        console.error("Magnifier Error: State image not found.");
        return;
    }
    
    // 从 box.box 中获取原始像素坐标和尺寸
    const { x: boxX, y: boxY, width: boxWidth, height: boxHeight } = box.box;

    if (boxWidth <= 0 || boxHeight <= 0) {
        return; // 无效尺寸，不执行任何操作
    }

    // --- 计算与图像边界相交的安全绘制区域 ---

    // 确保起始坐标不为负数
    const safeX = Math.max(0, boxX);
    const safeY = Math.max(0, boxY);

    // 计算在图像内的可用最大宽度和高度
    const availableWidth = state.image.width - safeX;
    const availableHeight = state.image.height - safeY;
    
    // 确定最终要绘制的宽度和高度，不能超过可用范围
    const drawWidth = Math.max(0, Math.min(boxWidth, availableWidth));
    const drawHeight = Math.max(0, Math.min(boxHeight, availableHeight));

    // 如果计算出的可绘制区域尺寸为0，则不显示
    if (drawWidth === 0 || drawHeight === 0) {
        hideMagnifier();
        return;
    }
    
    canvas.width = drawWidth;
    canvas.height = drawHeight;
    
    ctx.drawImage(
        state.image,
        safeX, safeY, drawWidth, drawHeight,    
        0, 0, drawWidth, drawHeight           
    );

    // （可选）在画布边缘绘制一个内边框，视觉效果更好
    ctx.strokeStyle = CONFIG.BOX_COLORS.selected;
    ctx.lineWidth = 2;
    // 在画布的边缘内侧1px处绘制，防止边框被裁掉
    ctx.strokeRect(1, 1, drawWidth - 2, drawHeight - 2); 
}
// 初始化应用
init();