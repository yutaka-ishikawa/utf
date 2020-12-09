BEGIN	{ FS = ","; m_idx = 0; f_idx = 0; u_idx = 0;
	}
/@pingpong-mpich/ {
	    name = $3; count = $4; size = $5; lat = $6; bw = $7;
	    ## printf " count=%s, size=%d, latency=%.4f, bw = %.4f\n", count, size, lat, bw;
	    m_sz[m_idx] = size; m_lat[m_idx]  = lat; m_bw[m_idx]  = bw;
	    m_idx++;
	}
/@pingpong-fjmpi/ {
	    name = $3; count = $4; size = $5; lat = $6; bw = $7;
	    ## printf " count=%s, size=%d, latency=%.4f, bw = %.4f\n", count, size, lat, bw;
	    f_sz[f_idx] = size; f_lat[f_idx]  = lat; f_bw[f_idx]  = bw;
	    f_idx++;
	}
/@pingpong-utf/ {
	    size = $2; lat = $3; bw = $4;
	    ## printf " count=%s, size=%d, latency=%.4f, bw = %.4f\n", count, size, lat, bw;
	    u_sz[u_idx] = size; u_lat[u_idx]  = lat; u_bw[u_idx]  = bw;
	    u_idx++;
	}
END	{
	  if (u_idx > 0) {
	      printf "#size, m-latency, f-latency, u-latency, m-bw, f-bw, u-bw\n";
	      for (i = 0; i < m_idx; i++) {
		  printf "%d, %.4f, %.4f, %.4f, %.4f, %.4f, %.4f\n",
		      m_sz[i], m_lat[i], f_lat[i], u_lat[i], m_bw[i], f_bw[i], u_bw[i];
	      }
	  } else {
	      printf "#size, m-latency, f-latency, m-bw, f-bw\n";
	      for (i = 0; i < m_idx; i++) {
		  printf "%d, %.4f, %.4f, %.4f, %.4f\n", m_sz[i], m_lat[i], f_lat[i], m_bw[i], f_bw[i];
	      }
	  }
	}
