import http from 'k6/http';
import { check, sleep } from 'k6';

export const options = {
  vus: 10000,
  duration: '1m',
  thresholds: {
    'http_req_duration': ['p(95)<500'],
    'http_req_failed': ['rate<0.01'],
    'checks{check:status is 200}': ['rate>0.99'],
    'checks{check:contains html}': ['rate>0.99'],
  },
};

const ROOT_URL = 'http://127.0.0.1:80/';

export default function () {
  const res = http.get(ROOT_URL);

  check(res, {
    'status is 200': (r) => r.status === 200,
    'content type is text/html': (r) => r.headers['Content-Type'].startsWith('text/html'),
  });

  sleep(1);
}
