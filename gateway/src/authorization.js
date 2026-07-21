import {timingSafeEqual} from 'node:crypto';

export function isAuthorized(header, expectedToken) {
  if (!expectedToken) {
    return true;
  }
  const provided = header?.replace(/^Bearer\s+/i, '') ?? '';
  const left = Buffer.from(provided);
  const right = Buffer.from(expectedToken);
  return left.length === right.length && timingSafeEqual(left, right);
}
