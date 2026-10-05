export default {
  fetch(request, env) {
    const target = new URL(request.url);
    if (target.protocol === 'http:') {
      target.protocol = 'https:';
      target.port = '';
      return Response.redirect(target.toString(), 308);
    }
    target.protocol = 'https:';
    target.hostname = new URL(env.ORIGIN).hostname;
    target.port = '';
    const outgoing = new Request(target, request);
    outgoing.headers.set('Host', target.host);
    return fetch(outgoing, {redirect: 'manual'});
  },
};
