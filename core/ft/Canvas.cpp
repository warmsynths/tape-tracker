#include "Canvas.h"

namespace ft
{
void Canvas::clear()
{
    count = 0;
    poolUsed = 0;
    over = false;
}

void Canvas::add(Op op, int a, int b, int c, int d, int off, int cost, int n)
{
    if (count >= kMaxCommands)
    {
        over = true;
        return;
    }
    Cmd& m = cmds[count++];
    m.op = op;
    m.a = (int16_t)a; m.b = (int16_t)b; m.c = (int16_t)c; m.d = (int16_t)d;
    m.off = (uint16_t)off; m.n = (uint16_t)n; m.cost = (int16_t)cost;
    if (op == kColor)
    {
        // colours do not fit in 16 bits: keep them in the pool
        if (poolUsed + 4 > kPoolSize) { over = true; count--; return; }
        const uint32_t v = (uint32_t)a;
        m.off = (uint16_t)poolUsed;
        pool[poolUsed++] = (uint8_t)(v >> 16); pool[poolUsed++] = (uint8_t)(v >> 8); pool[poolUsed++] = (uint8_t)v; pool[poolUsed++] = 0;
    }
}

void Canvas::text(int x, int y, const char* s, int font)
{
    int len = 0;
    while (s[len] != 0) len++;
    if (len == 0) return;
    if (poolUsed + len > kPoolSize) { over = true; return; }
    const int off = poolUsed;
    for (int i = 0; i < len; i++) pool[poolUsed++] = (uint8_t)s[i];
    add(kText, x, y, font, 0, off, 16 + len * (x > 480 ? 8 : 4), len);
}

void Canvas::bars(int x0, int cy, int step, int bw, const uint8_t* heights, int n)
{
    if (poolUsed + n > kPoolSize) { over = true; return; }
    const int off = poolUsed;
    for (int i = 0; i < n; i++) pool[poolUsed++] = heights[i];
    add(kBars, x0, cy, step, bw, off, 12 + n * 8, n);
}

int Canvas::estimatedBytes() const
{
    int total = 0;
    for (int i = 0; i < count; i++) total += cmds[i].cost;
    return total;
}

// A stage fills up to the budget and starts at a colour command, so a colour is never split
// from the command that uses it.

int Canvas::stageCount() const
{
    if (count == 0) return 0;
    int stages = 1, used = 0;
    for (int i = 0; i < count; i++)
    {
        if (cmds[i].op == kColor && used + cmds[i].cost > 0 && used > kStageBudget)
        {
            stages++;
            used = 0;
        }
        used += cmds[i].cost;
    }
    return stages > kMaxStages ? kMaxStages : stages;
}

void Canvas::replay(int stage, Gfx& g) const
{
    int s = 0, used = 0;
    // The stage count is capped; the last stage takes the rest.
    const int stages = stageCount();
    for (int i = 0; i < count; i++)
    {
        if (cmds[i].op == kColor && used > kStageBudget && s < stages - 1)
        {
            s++;
            used = 0;
        }
        used += cmds[i].cost;
        if (s != stage) continue;
        const Cmd& m = cmds[i];
        switch (m.op)
        {
            case kColor: g.color(((uint32_t)pool[m.off] << 16) | ((uint32_t)pool[m.off + 1] << 8) | pool[m.off + 2]); break;
            case kAlpha: g.alpha(m.a); break;
            case kRect: g.rect(m.a, m.b, m.c, m.d); break;
            case kLine: g.line(m.a, m.b, m.c, m.d); break;
            case kDot: g.dot(m.a, m.b, m.c); break;
            case kText:
            {
                char buf[96];
                int n = m.n < 95 ? m.n : 95;
                for (int k = 0; k < n; k++) buf[k] = (char)pool[m.off + k];
                buf[n] = 0;
                g.text(m.a, m.b, buf, m.c);
                break;
            }
            case kBars: g.bars(m.a, m.b, m.c, m.d, pool + m.off, m.n); break;
        }
    }
}
} // namespace ft
