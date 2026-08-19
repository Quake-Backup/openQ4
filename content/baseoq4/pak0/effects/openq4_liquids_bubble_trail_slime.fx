effect effects/openq4/liquids/bubble_trail_slime
{
	size 512
	spawner "toxic bubbles"
	{
		count 16,24
		sprite
		{
			duration 0.55,1.1
			persist
			material "gfx/effects/fluids_drips/bubble_green_half"
			start
			{
				position { line 0,-3,-3,0,3,3 useEndOrigin linearSpacing }
				velocity { box 0,-2,-2,3,2,2 }
				size { box 1.5,1.5,3.5,3.5 }
				tint { line 0.3,0.9,0.08,0.65,1,0.18 }
				fade { point 0.85 }
			}
			motion
			{
				size { envelope linear }
				fade { envelope linear }
			}
			end
			{
				size { box 3,3,6,6 }
				fade { point 0.05 }
			}
		}
	}
}
