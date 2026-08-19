effect effects/openq4/liquids/bubble_trail_water
{
	size 512
	spawner "bubbles"
	{
		count 14,22
		sprite
		{
			duration 0.45,0.9
			persist
			material "gfx/effects/fluids_drips/bubble_alpha"
			start
			{
				position { line 0,-2,-2,0,2,2 useEndOrigin linearSpacing }
				velocity { box 0,-3,-3,5,3,3 }
				size { box 1,1,2.5,2.5 }
				fade { point 0.8 }
			}
			motion
			{
				size { envelope linear }
				fade { envelope linear }
			}
			end
			{
				size { box 2,2,4,4 }
				fade { point 0.05 }
			}
		}
	}
}
