// Static CanvasWidget drawing helpers shared by the GPS HUD and big map.
// Callers own the canvas lifecycle; this class only projects, clips and draws.

class BDE_EGPS_RouteDrawer
{

    // Cohen-Sutherland outcode bits for canvas-local coordinates.
    protected static const int OC_LEFT   = 1;
    protected static const int OC_RIGHT  = 2;
    protected static const int OC_BOTTOM = 4;
    protected static const int OC_TOP    = 8;

    // Draws the route path, player lead-in and waypoint tails in one pass.
    static void DrawFullRoute(
        MapWidget mapWidget,
        CanvasWidget canvas,
        array<vector> pathPositions,
        int           traversedIdx,
        vector        playerPos,
        array<ref BDE_EGPS_Waypoint> waypoints,
        array<int>    segmentEndIndices,  // waypoint subsegment bounds
        array<vector> tailStartPath,      // ROAD-mode dirt tail at start
        array<vector> tailEndPath,        // ROAD-mode dirt tail at end
        BDE_EGPS_DrawStyle style,
        float         canvasOffsetX,
        float         canvasOffsetY,
        float         canvasW,
        float         canvasH)
    {
        if (!mapWidget || !canvas || !style) return;

        int   colorPending         = style.m_ColorPending;
        int   colorTraversed       = style.m_ColorTraversed;
        int   colorDashed          = style.m_ColorDashed;
        float thicknessMain        = style.m_ThicknessMain;
        float thicknessDashed      = style.m_ThicknessDashed;
        float dashSegPx            = style.m_DashSegPx;
        float dashGapPx            = style.m_DashGapPx;
        float dashedTailThresholdM = style.m_DashedTailThresholdM;

        // Last-resort fallback when no road path is available.
        if (!pathPositions || pathPositions.Count() < 2)
        {
            if (waypoints && waypoints.Count() > 0)
            {
                DrawDashedFallback(mapWidget, canvas, playerPos, waypoints,
                    colorDashed, thicknessDashed, dashSegPx, dashGapPx,
                    canvasOffsetX, canvasOffsetY, canvasW, canvasH);
            }
            return;
        }

        int n = pathPositions.Count();
        int idx = traversedIdx;
        if (idx < 0) idx = 0;
        if (idx > n - 1) idx = n - 1;

        // Two-colour road path.
        DrawPathSegments(mapWidget, canvas, pathPositions, 0, idx,
            colorTraversed, thicknessMain, canvasOffsetX, canvasOffsetY, canvasW, canvasH);
        DrawPathSegments(mapWidget, canvas, pathPositions, idx, n - 1,
            colorPending, thicknessMain, canvasOffsetX, canvasOffsetY, canvasW, canvasH);

        // Head dashed line: use the ROAD-mode tail only before the player
        // reaches the road. After that, draw from the live player position.
        vector firstPendingPos = pathPositions.Get(idx);
        if (vector.DistanceSq(playerPos, firstPendingPos) > 25.0)  // > 5m
        {
            if (idx == 0 && tailStartPath && tailStartPath.Count() >= 2)
            {
                DrawPolyDashed(mapWidget, canvas, tailStartPath,
                    thicknessDashed, colorDashed, dashSegPx, dashGapPx,
                    canvasOffsetX, canvasOffsetY, canvasW, canvasH);
            }
            else
            {
                vector playerScreen = mapWidget.MapToScreen(playerPos);
                vector firstScreen  = mapWidget.MapToScreen(firstPendingPos);
                DrawDashedLine(canvas,
                    playerScreen[0] - canvasOffsetX, playerScreen[1] - canvasOffsetY,
                    firstScreen[0]  - canvasOffsetX, firstScreen[1]  - canvasOffsetY,
                    thicknessDashed, colorDashed, dashSegPx, dashGapPx,
                    canvasW, canvasH);
            }
        }

        // Dashed tails for off-road waypoints.
        if (waypoints && waypoints.Count() > 0 && segmentEndIndices && segmentEndIndices.Count() == waypoints.Count())
        {
            int nWp = waypoints.Count();
            int wpI;
            BDE_EGPS_Waypoint wp;
            for (wpI = 0; wpI < nWp; wpI++)
            {
                wp = waypoints.Get(wpI);
                if (!wp) continue;
                vector wpPos = wp.GetPos();

                // Limit the tail search to this waypoint's own segment.
                int segStart = 0;
                if (wpI > 0) segStart = segmentEndIndices.Get(wpI - 1);
                int segEnd = segmentEndIndices.Get(wpI);

                if (segStart < 0) segStart = 0;
                if (segEnd >= n) segEnd = n - 1;
                if (segStart > segEnd) continue;  // invalid subsegment (should not happen)

                int   nearestIdx  = -1;
                float nearestDist = 99999999.0;
                int   pi;
                for (pi = segStart; pi <= segEnd; pi++)
                {
                    float d = vector.DistanceSq(wpPos, pathPositions.Get(pi));
                    if (d < nearestDist)
                    {
                        nearestDist = d;
                        nearestIdx  = pi;
                    }
                }
                if (nearestIdx == -1) continue;

                float realDist = Math.Sqrt(nearestDist);
                if (realDist < dashedTailThresholdM) continue;  // waypoint sits on the path

                bool isLastWp = (wpI == nWp - 1);
                if (isLastWp && tailEndPath && tailEndPath.Count() >= 2)
                {
                    DrawPolyDashed(mapWidget, canvas, tailEndPath,
                        thicknessDashed, colorDashed, dashSegPx, dashGapPx,
                        canvasOffsetX, canvasOffsetY, canvasW, canvasH);
                }
                else
                {
                    vector nearestPos = pathPositions.Get(nearestIdx);
                    vector nearestScreen = mapWidget.MapToScreen(nearestPos);
                    vector wpScreen      = mapWidget.MapToScreen(wpPos);
                    DrawDashedLine(canvas,
                        nearestScreen[0] - canvasOffsetX, nearestScreen[1] - canvasOffsetY,
                        wpScreen[0]      - canvasOffsetX, wpScreen[1]      - canvasOffsetY,
                        thicknessDashed, colorDashed, dashSegPx, dashGapPx,
                        canvasW, canvasH);
                }
            }
        }
    }

    // Draws a dashed polyline through world positions.
    protected static void DrawPolyDashed(
        MapWidget mapWidget, CanvasWidget canvas, array<vector> positions,
        float thickness, int color, float dashSegPx, float dashGapPx,
        float canvasOffsetX, float canvasOffsetY,
        float canvasW, float canvasH)
    {
        if (!mapWidget || !canvas || !positions) return;
        int n = positions.Count();
        if (n < 2) return;

        vector prevScreen = mapWidget.MapToScreen(positions.Get(0));
        vector curScreen;
        int i;
        for (i = 1; i < n; i++)
        {
            curScreen = mapWidget.MapToScreen(positions.Get(i));
            DrawDashedLine(canvas,
                prevScreen[0] - canvasOffsetX, prevScreen[1] - canvasOffsetY,
                curScreen[0]  - canvasOffsetX, curScreen[1]  - canvasOffsetY,
                thickness, color, dashSegPx, dashGapPx,
                canvasW, canvasH);
            prevScreen = curScreen;
        }
    }

    // Dashed-only fallback for routes with waypoints but no road path.
    protected static void DrawDashedFallback(
        MapWidget mapWidget, CanvasWidget canvas,
        vector playerPos, array<ref BDE_EGPS_Waypoint> waypoints,
        int color, float thickness, float dashSegPx, float dashGapPx,
        float canvasOffsetX, float canvasOffsetY,
        float canvasW, float canvasH)
    {
        if (!waypoints) return;
        int n = waypoints.Count();
        if (n == 0) return;

        vector prevPos = playerPos;
        vector prevScreen;
        vector wpScreen;
        vector wpPos;
        BDE_EGPS_Waypoint wp;
        int i;
        for (i = 0; i < n; i++)
        {
            wp = waypoints.Get(i);
            if (!wp) continue;
            wpPos = wp.GetPos();

            if (vector.DistanceSq(prevPos, wpPos) < 25.0)
            {
                prevPos = wpPos;
                continue;
            }
            prevScreen = mapWidget.MapToScreen(prevPos);
            wpScreen   = mapWidget.MapToScreen(wpPos);
            DrawDashedLine(canvas,
                prevScreen[0] - canvasOffsetX, prevScreen[1] - canvasOffsetY,
                wpScreen[0]   - canvasOffsetX, wpScreen[1]   - canvasOffsetY,
                thickness, color, dashSegPx, dashGapPx,
                canvasW, canvasH);
            prevPos = wpPos;
        }
    }

    // Draws consecutive path segments in [fromIdx, toIdx].
    protected static void DrawPathSegments(
        MapWidget mapWidget, CanvasWidget canvas, array<vector> pathPositions,
        int fromIdx, int toIdx, int color, float thickness,
        float canvasOffsetX, float canvasOffsetY,
        float canvasW, float canvasH)
    {
        if (toIdx <= fromIdx) return;
        vector prevScreen = mapWidget.MapToScreen(pathPositions.Get(fromIdx));
        vector curScreen;
        int i;
        float ax, ay, bx, by;
        for (i = fromIdx + 1; i <= toIdx; i++)
        {
            curScreen = mapWidget.MapToScreen(pathPositions.Get(i));
            ax = prevScreen[0] - canvasOffsetX;
            ay = prevScreen[1] - canvasOffsetY;
            bx = curScreen[0]  - canvasOffsetX;
            by = curScreen[1]  - canvasOffsetY;
            DrawLineClipped(canvas, ax, ay, bx, by, thickness, color, canvasW, canvasH);
            prevScreen = curScreen;
        }
    }

    // Draws a clipped dashed line in canvas-local coordinates.
    protected static void DrawDashedLine(
        CanvasWidget canvas,
        float x1, float y1, float x2, float y2,
        float thickness, int color,
        float segPx, float gapPx,
        float canvasW, float canvasH)
    {
        if (!canvas) return;

        float dx = x2 - x1;
        float dy = y2 - y1;
        float lenTotal = Math.Sqrt(dx * dx + dy * dy);
        if (lenTotal < 1.0) return;

        if (segPx <= 0.0) return;
        if (gapPx < 0.0) gapPx = 0.0;
        float patternLen = segPx + gapPx;
        if (patternLen <= 0.0) return;

        if (lenTotal < patternLen)
        {
            DrawLineClipped(canvas, x1, y1, x2, y2, thickness, color, canvasW, canvasH);
            return;
        }

        float ux = dx / lenTotal;
        float uy = dy / lenTotal;

        float visibleX1 = x1;
        float visibleY1 = y1;
        float visibleX2 = x2;
        float visibleY2 = y2;
        if (canvasW > 0.0 && canvasH > 0.0)
        {
            if (!ClipLineCS(visibleX1, visibleY1, visibleX2, visibleY2, canvasW, canvasH))
                return;
        }

        float visibleStart = (visibleX1 - x1) * ux + (visibleY1 - y1) * uy;
        float visibleEnd   = (visibleX2 - x1) * ux + (visibleY2 - y1) * uy;
        if (visibleEnd < visibleStart)
        {
            float tmp = visibleStart;
            visibleStart = visibleEnd;
            visibleEnd = tmp;
        }
        if (visibleStart < 0.0) visibleStart = 0.0;
        if (visibleEnd > lenTotal) visibleEnd = lenTotal;
        if (visibleEnd - visibleStart < 1.0) return;

        int dashIdx = Math.Floor(visibleStart / patternLen);
        float traveled = dashIdx * patternLen;
        float sx, sy, ex, ey;
        float dashEnd;
        float drawStart;
        float drawEnd;
        while (traveled < visibleEnd)
        {
            dashEnd = traveled + segPx;
            if (dashEnd > visibleStart && traveled < visibleEnd)
            {
                drawStart = traveled;
                if (drawStart < visibleStart) drawStart = visibleStart;
                drawEnd = dashEnd;
                if (drawEnd > visibleEnd) drawEnd = visibleEnd;

                sx = x1 + ux * drawStart;
                sy = y1 + uy * drawStart;
                ex = x1 + ux * drawEnd;
                ey = y1 + uy * drawEnd;
                DrawLineClipped(canvas, sx, sy, ex, ey, thickness, color, canvasW, canvasH);
            }
            traveled = traveled + patternLen;
        }
    }

    // Clips and draws one canvas-local segment.
    protected static void DrawLineClipped(
        CanvasWidget canvas,
        float x1, float y1, float x2, float y2,
        float thickness, int color,
        float xmax, float ymax)
    {
        if (!canvas) return;
        if (xmax <= 0.0 || ymax <= 0.0)
        {
            canvas.DrawLine(x1, y1, x2, y2, thickness, color);
            return;
        }
        if (ClipLineCS(x1, y1, x2, y2, xmax, ymax))
            canvas.DrawLine(x1, y1, x2, y2, thickness, color);
    }

    // Cohen-Sutherland line clip against [0, xmax] x [0, ymax].
    protected static bool ClipLineCS(
        inout float x1, inout float y1, inout float x2, inout float y2,
        float xmax, float ymax)
    {
        int oc1 = ComputeOutCode(x1, y1, xmax, ymax);
        int oc2 = ComputeOutCode(x2, y2, xmax, ymax);

        int iters = 0;
        while (iters < 8)
        {
            iters++;
            if ((oc1 | oc2) == 0) return true;       // both inside
            if ((oc1 & oc2) != 0) return false;      // share an outside edge

            int ocOut;
            if (oc1 != 0) ocOut = oc1;
            else          ocOut = oc2;

            float xClip = 0.0;
            float yClip = 0.0;
            float dxLine = x2 - x1;
            float dyLine = y2 - y1;

            if ((ocOut & OC_TOP) != 0)
            {
                if (dyLine != 0.0)
                    xClip = x1 + dxLine * (ymax - y1) / dyLine;
                else
                    xClip = x1;
                yClip = ymax;
            }
            else if ((ocOut & OC_BOTTOM) != 0)
            {
                if (dyLine != 0.0)
                    xClip = x1 + dxLine * (0.0 - y1) / dyLine;
                else
                    xClip = x1;
                yClip = 0.0;
            }
            else if ((ocOut & OC_RIGHT) != 0)
            {
                if (dxLine != 0.0)
                    yClip = y1 + dyLine * (xmax - x1) / dxLine;
                else
                    yClip = y1;
                xClip = xmax;
            }
            else if ((ocOut & OC_LEFT) != 0)
            {
                if (dxLine != 0.0)
                    yClip = y1 + dyLine * (0.0 - x1) / dxLine;
                else
                    yClip = y1;
                xClip = 0.0;
            }

            if (ocOut == oc1)
            {
                x1 = xClip;
                y1 = yClip;
                oc1 = ComputeOutCode(x1, y1, xmax, ymax);
            }
            else
            {
                x2 = xClip;
                y2 = yClip;
                oc2 = ComputeOutCode(x2, y2, xmax, ymax);
            }
        }
        return false;
    }

    protected static int ComputeOutCode(float x, float y, float xmax, float ymax)
    {
        int code = 0;
        if (x < 0.0)  code = code | OC_LEFT;
        if (x > xmax) code = code | OC_RIGHT;
        if (y < 0.0)  code = code | OC_BOTTOM;
        if (y > ymax) code = code | OC_TOP;
        return code;
    }
}
