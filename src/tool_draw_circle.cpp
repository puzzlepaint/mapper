/*
 *    Copyright 2012, 2013 Thomas Schöps
 *
 *    This file is part of OpenOrienteering.
 *
 *    OpenOrienteering is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    OpenOrienteering is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with OpenOrienteering.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "tool_draw_circle.h"

#if QT_VERSION < 0x050000
#include <QtGui>
#else
#include <QtWidgets>
#endif

#include "map.h"
#include "object.h"
#include "util.h"
#include "gui/modifier_key.h"

QCursor* DrawCircleTool::cursor = NULL;

DrawCircleTool::DrawCircleTool(MapEditorController* editor, QAction* tool_button, SymbolWidget* symbol_widget)
 : DrawLineAndAreaTool(editor, DrawCircle, tool_button, symbol_widget)
{
	dragging = false;
	first_point_set = false;
	second_point_set = false;
	
	if (!cursor)
		cursor = new QCursor(QPixmap(":/images/cursor-draw-circle.png"), 11, 11);
}

void DrawCircleTool::init()
{
	updateStatusText();
}

bool DrawCircleTool::mousePressEvent(QMouseEvent* event, MapCoordF map_coord, MapWidget* widget)
{
	Q_UNUSED(widget);
	
	if ((event->button() == Qt::LeftButton) || (draw_in_progress && drawMouseButtonClicked(event)))
	{
		cur_pos = event->pos();
		cur_pos_map = map_coord;
		
		if (!first_point_set && event->buttons() & Qt::LeftButton)
		{
			click_pos = event->pos();
			circle_start_pos_map = map_coord;
			opposite_pos_map = map_coord;
			dragging = false;
			first_point_set = true;
			start_from_center = event->modifiers() & Qt::ControlModifier;
			
			if (!draw_in_progress)
				startDrawing();
		}
		else if (first_point_set && !second_point_set)
		{
			click_pos = event->pos();
			opposite_pos_map = map_coord;
			dragging = false;
			second_point_set = true;
		}
		else
			return false;
		
		hidePreviewPoints();
		return true;
	}
	else if (event->button() == Qt::RightButton && draw_in_progress)
	{
		abortDrawing();
		return true;
	}
	return false;
}

bool DrawCircleTool::mouseMoveEvent(QMouseEvent* event, MapCoordF map_coord, MapWidget* widget)
{
	Q_UNUSED(widget);
	
	bool mouse_down = drawMouseButtonHeld(event);
	
	if (!mouse_down)
	{
		if (!draw_in_progress)
		{
			setPreviewPointsPosition(map_coord);
			setDirtyRect();
		}
		else
		{
			cur_pos = event->pos();
			cur_pos_map = map_coord;
			if (!second_point_set)
				opposite_pos_map = map_coord;
			updateCircle();
		}
	}
	else
	{
		if (!draw_in_progress)
			return false;
		
		if ((event->pos() - click_pos).manhattanLength() >= QApplication::startDragDistance())
		{
			if (!dragging)
			{
				dragging = true;
				updateStatusText();
			}
		}
		
		if (dragging)
		{
			cur_pos = event->pos();
			cur_pos_map = map_coord;
			if (!second_point_set)
				opposite_pos_map = cur_pos_map;
			updateCircle();
		}
	}
	return true;
}

bool DrawCircleTool::mouseReleaseEvent(QMouseEvent* event, MapCoordF map_coord, MapWidget* widget)
{
	Q_UNUSED(widget);
	
	if (!drawMouseButtonClicked(event))
	{
		if (event->button() == Qt::RightButton)
			abortDrawing();
		return false;
	}
	if (!draw_in_progress)
		return false;
	
	updateStatusText();
	
	if (dragging || second_point_set)
	{
		cur_pos = event->pos();
		cur_pos_map = map_coord;
		if (dragging && !second_point_set)
			opposite_pos_map = map_coord;
		updateCircle();
		finishDrawing();
		return true;
	}
	return false;
}

bool DrawCircleTool::keyPressEvent(QKeyEvent* event)
{
	if (event->key() == Qt::Key_Escape)
		abortDrawing();
	else if (event->key() == Qt::Key_Tab)
	{
		deactivate();
		return true;
	}
	return false;
}

void DrawCircleTool::draw(QPainter* painter, MapWidget* widget)
{
	drawPreviewObjects(painter, widget);
}

void DrawCircleTool::finishDrawing()
{
	dragging = false;
	first_point_set = false;
	second_point_set = false;
	updateStatusText();
	
	DrawLineAndAreaTool::finishDrawing();
	// Do not add stuff here as the tool might get deleted in DrawLineAndAreaTool::finishDrawing()!
}
void DrawCircleTool::abortDrawing()
{
	dragging = false;
	first_point_set = false;
	second_point_set = false;
	updateStatusText();
	
	DrawLineAndAreaTool::abortDrawing();
}

void DrawCircleTool::updateCircle()
{
	MapCoordF first_pos_map;
	if (start_from_center)
		first_pos_map = circle_start_pos_map + (circle_start_pos_map - opposite_pos_map);
	else
		first_pos_map = circle_start_pos_map;
	
	float radius = 0.5f * first_pos_map.lengthTo(opposite_pos_map);
	float kappa = BEZIER_KAPPA;
	float m_kappa = 1 - BEZIER_KAPPA;
	
	MapCoordF across = opposite_pos_map - first_pos_map;
	across.toLength(radius);
	MapCoordF right = across;
	right.perpRight();
	
	float right_radius = radius;
	if (second_point_set && dragging)
	{
		if (right.length() < 1e-8)
			right_radius = 0;
		else
		{
			MapCoordF to_cursor = cur_pos_map - first_pos_map;
			right_radius = to_cursor.dot(right) / right.length();
		}
	}
	right.toLength(right_radius);
	
	preview_path->clearCoordinates();
	preview_path->addCoordinate(first_pos_map.toCurveStartMapCoord());
	preview_path->addCoordinate((first_pos_map + kappa * right).toMapCoord());
	preview_path->addCoordinate((first_pos_map + right + m_kappa * across).toMapCoord());
	preview_path->addCoordinate((first_pos_map + right + across).toCurveStartMapCoord());
	preview_path->addCoordinate((first_pos_map + right + (1 + kappa) * across).toMapCoord());
	preview_path->addCoordinate((first_pos_map + kappa * right + 2 * across).toMapCoord());
	preview_path->addCoordinate((first_pos_map + 2 * across).toCurveStartMapCoord());
	preview_path->addCoordinate((first_pos_map - kappa * right + 2 * across).toMapCoord());
	preview_path->addCoordinate((first_pos_map - right + (1 + kappa) * across).toMapCoord());
	preview_path->addCoordinate((first_pos_map - right + across).toCurveStartMapCoord());
	preview_path->addCoordinate((first_pos_map - right + m_kappa * across).toMapCoord());
	preview_path->addCoordinate((first_pos_map - kappa * right).toMapCoord());
	preview_path->getPart(0).setClosed(true, false);
	
	updatePreviewPath();
	setDirtyRect();
}

void DrawCircleTool::setDirtyRect()
{
	QRectF rect;
	includePreviewRects(rect);
	
	if (is_helper_tool)
		emit(dirtyRectChanged(rect));
	else
	{
		if (rect.isValid())
			map()->setDrawingBoundingBox(rect, 0, true);
		else
			map()->clearDrawingBoundingBox();
	}
}

void DrawCircleTool::updateStatusText()
{
	QString text;
	if (!first_point_set || (!second_point_set && dragging))
	{
		text = tr("<b>Click</b>: Start a circle or ellipse. ") + 
		       tr("<b>Drag</b>: Draw a circle. ") +
			   "| " +
			   tr("Hold %1 to start drawing from the center.").arg(ModifierKey::control());
	}
	else
	{
		text = tr("<b>Click</b>: Finish the circle. ") +
		       tr("<b>Drag</b>: Draw an ellipse. ") +
		       MapEditorTool::tr("<b>%1</b>: Abort. ").arg(ModifierKey::escape());
	}
	setStatusBarText(text);
}