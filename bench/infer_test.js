import http from 'k6/http';
import { check, sleep } from 'k6';

export const options = {
  vus: 233,
  duration: '1m',
  thresholds: {
    'http_req_duration': ['p(95)<5000'],
    'http_req_failed': ['rate<0.01'],
    'checks{check:status is 200}': ['rate>0.99'],
  },
};

const imageFile = open('../tests/images/hand/f10.6.jpg', 'b');

const API_URL = 'http://127.0.0.1:80/predict';


export default function () {
  const data = {
    image: http.file(imageFile, 'test-image.jpg', 'image/jpeg'),
  };

  const res = http.post(API_URL, data);

  check(res, {
    'status is 200': (r) => r.status === 200
  });

  sleep(1);
}