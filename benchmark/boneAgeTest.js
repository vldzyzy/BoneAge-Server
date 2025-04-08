import http from 'k6/http'; // 导入 k6 http 模块，用于发送 HTTP 请求
import { check, sleep } from 'k6'; // 导入 check 用于断言，sleep 用于暂停

// --- 测试配置 ---
export const options = {
  vus: 100,           // 模拟 4 个并发用户
  //duration: '60s',   // 测试运行 30 秒
  iterations: 200, // 或者，总共运行 100 次迭代

  thresholds: {
    // 定义通过/失败的标准
    'http_req_failed': ['rate<0.01'], // 如果错误率 > 1%，则测试失败
    'http_req_duration': ['p(95)<5000'], // 如果 95% 的请求响应时间 > 1000毫秒 (1秒)，则失败
    'checks': ['rate>0.98'], // 如果检查成功率 < 98%，则失败
  },
};

// --- 准备数据 ---
// 读取图像文件的二进制内容。
// 确保 'test_image.jpg' 与脚本在同一目录，或者提供正确的路径。
// 'b' 模式对于读取二进制文件至关重要。
const imageFileData = open('1377.png', 'b');

// --- 主要测试逻辑 (由每个 VU 执行) ---
export default function () {
  const url = 'http://127.0.0.1:80/detect'; // 您的 API 端点

  // --- 准备 multipart/form-data 载荷 ---
  // 当您传递一个对象作为 payload 时，k6 会自动处理 boundary 等。
  const payload = {
    // 'image_file' 这个键应该匹配您的服务器期望接收的文件上传字段的名称。
    // http.file(二进制数据, 文件名, 内容类型)
    image: http.file(imageFileData, '1377.png', 'image/png'),
    // 如果需要，您可以在这里添加其他表单字段：
    // another_field: 'some_value',
  };

  // --- 准备请求头 ---
  // 将 <your_token> 替换为实际有效的 token。
  // 考虑使用环境变量来存储 token 等敏感数据！
  const params = {
    headers: {
      // 'Authorization': 'Bearer <your_token>',
      // 当使用对象作为 payload 时，k6 会自动为 multipart/form-data 设置 'Content-Type'。
      // 除非有特殊原因，否则不要手动设置它。
    },
  };

  // --- 发送 POST 请求 ---
  const res = http.post(url, payload, params);

  // --- 检查响应 ---
  check(res, {
    '状态码是 200': (r) => r.status === 200, // 检查 HTTP 状态码是否为 200
    '响应体不为空': (r) => r.body.length > 0, // 对响应进行基本检查
    // 如果可能，添加更具体的检查，例如检查 JSON 响应中的某个键：
    // '响应包含结果': (r) => r.json('prediction_key') !== undefined,
  });

  // --- 可选的思考时间 ---
  // 模拟用户在两次请求之间暂停 1 秒
  // sleep(1);
}

// --- 可选的 setup/teardown 函数 ---
// export function setup() {
//   // 在测试开始前运行一次。可用于登录、准备数据等。
//   console.log('正在设置测试...');
//   // setup 函数可以返回数据，供 default 函数使用
//   // return { setupData: 'prepared_value' };
// }

// export function teardown(data) {
//   // 在测试结束后运行一次。可用于清理资源。
//   console.log('正在清理测试...');
// }
