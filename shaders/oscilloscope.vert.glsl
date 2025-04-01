/*
   Copyright (c) 2019 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#version 130

in vec3 position;

void main()
{
	gl_Position = vec4(position, 1.0); 
}
