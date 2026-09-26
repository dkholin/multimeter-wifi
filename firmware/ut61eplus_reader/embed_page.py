# Generates src/page.c from docs/index.html so firmware and GitHub Pages serve the same dashboard.
Import("env")
import os
root = env.subst("$PROJECT_DIR")
data = open(os.path.join(root, "..", "..", "docs", "index.html"), "rb").read()
out = os.path.join(root, "src", "page.c")
body = ",".join(str(b) for b in data)
new = "const unsigned char page_html[] = {%s,0};\nconst unsigned int page_html_len = %d;\n" % (body, len(data))
if not os.path.exists(out) or open(out).read() != new:
    open(out, "w").write(new)
