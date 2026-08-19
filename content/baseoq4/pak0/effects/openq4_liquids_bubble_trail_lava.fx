effect effects/openq4/liquids/bubble_trail_lava
{
	size 512
	spawner "hot bubbles"
	{
		count 12,18
		sprite
		{
			duration 0.3,0.7
			persist
			material "gfx/effects/fluids_drips/bubble_red_half"
			start
			{
				position { line 0,-2,-2,0,2,2 useEndOrigin linearSpacing }
				velocity { box 0,-2,-2,4,2,2 }
				size { box 1.5,1.5,3,3 }
				tint { line 1,0.35,0.05,1,0.8,0.18 }
				fade { point 0.9 }
			}
			motion
			{
				size { envelope linear }
				fade { envelope linear }
			}
			end
			{
				size { box 3,3,5,5 }
				fade { point 0.05 }
			}
		}
	}
}
