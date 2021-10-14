/*
    This source file is part of Rigs of Rods
    Copyright 2005-2012 Pierre-Michel Ricordel
    Copyright 2007-2012 Thomas Fischer
    Copyright 2013-2020 Petr Ohlidal

    For more information, see http://www.rigsofrods.org/

    Rigs of Rods is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 3, as
    published by the Free Software Foundation.

    Rigs of Rods is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Rigs of Rods. If not, see <http://www.gnu.org/licenses/>.
*/

/// @file
/// @author Petr Ohlidal
/// @date   04/2015

#include "RigDef_Node.h"

#include "Application.h"

using namespace RigDef;

// Ctors

Node::Id::Id():
    m_id_num(0)
{}

Node::Id::Id(unsigned int num):
    m_id_num(num)
{
    this->SetNum(num);    
}

Node::Id::Id(std::string const & id_str):
    m_id_num(0)
{
    this->setStr(id_str);
}

// Setters

void Node::Id::SetNum(unsigned int num)
{
    m_id_num = num;
    m_id_str = TOSTRING(num);
}

void Node::Id::setStr(std::string const & id_str)
{
    m_id_num = 0;
    m_id_str = id_str;
}

// Util


Node::Ref::Ref(std::string const & id_str, unsigned int id_num, unsigned flags, unsigned line_number):

    m_line_number(line_number),
    m_id_as_number(id_num)
{
    m_id = id_str;

}

Node::Ref::Ref():

    m_id_as_number(0),
    m_line_number(0)
{
}



std::string Node::Ref::ToString() const
{
    std::stringstream msg;
    msg << "Node::Ref(id:" << m_id
        << ", src line:";
    if (m_line_number != 0)
    {
        msg << m_line_number;
    }
    else
    {
        msg << "?";
    }
    msg << ")";
    return msg.str();
}

std::string Node::Id::ToString() const
{
    return std::string("Node::Id(") + this->Str() ;
}
