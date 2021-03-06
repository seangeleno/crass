/*
 *  CrisprGraph.cpp is part of the crisprtools project
 *  
 *  Created by Connor Skennerton on 7/12/11.
 *  Copyright 2011 Connor Skennerton. All rights reserved. 
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *
 *
 *                     A B R A K A D A B R A
 *                      A B R A K A D A B R
 *                       A B R A K A D A B
 *                        A B R A K A D A       	
 *                         A B R A K A D
 *                          A B R A K A
 *                           A B R A K
 *                            A B R A
 *                             A B R
 *                              A B
 *                               A
 */

#include <iostream>
#include "CrisprGraph.h"

namespace crispr {

        
    int graph::setNodeAttribute(Agnode_t * node ,char * attrName, char * attrValue)
    {
        char * dummy = strdup("");
        int ret = agsafeset(node, attrName, attrValue, dummy);
        delete dummy;
        return ret;
    }  
    int graph::setGraphAttribute(char * attrName, char * attrValue)
    {
        char * dummy = strdup("");
        int ret = agsafeset(G_graph, attrName, attrValue, dummy);
        delete dummy;
        return ret;
    }
    int graph::setEdgeAttribute(Agedge_t * edge ,char * attrName, char * attrValue)
    {
        char * dummy = strdup("");
        int ret = agsafeset(edge, attrName, attrValue, dummy);
        delete dummy;
        return ret;
    }
    


}
