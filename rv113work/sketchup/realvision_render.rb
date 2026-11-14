require 'sketchup.rb'
require 'json'
require 'date'
require 'socket'
require 'fileutils'
require 'tmpdir'
module RealVisionRender
  ROOT=File.dirname(__FILE__)
  def self.num(v); format('%.7f',v.to_f); end
  # NOAA-style solar position. Azimuth is clockwise from north (+Y toward +X),
  # matching the engine's sun convention. Timezone is local UTC offset in hours.
  def self.solar_position(date_text, time_text, latitude, longitude, timezone)
    date_parts=date_text.to_s.split('-').map(&:to_i)
    time_parts=time_text.to_s.split(':').map(&:to_i)
    raise 'Tanggal harus YYYY-MM-DD.' unless date_parts.length==3 && date_parts[0]>1900 && Date.valid_date?(*date_parts)
    raise 'Waktu harus HH:MM.' unless time_parts.length>=2 && time_parts[0].between?(0,23) && time_parts[1].between?(0,59)
    lat=Float(latitude); lon=Float(longitude); tz=Float(timezone)
    raise 'Latitude harus antara -90 dan 90.' unless lat.between?(-90.0,90.0)
    raise 'Longitude harus antara -180 dan 180.' unless lon.between?(-180.0,180.0)
    raise 'UTC offset harus antara -12 dan 14.' unless tz.between?(-12.0,14.0)
    year,month,day=date_parts
    hour=time_parts[0]+time_parts[1]/60.0+(time_parts[2]||0)/3600.0
    day_of_year=Time.utc(year,month,day).yday
    gamma=2.0*Math::PI/365.0*(day_of_year-1+(hour-12.0)/24.0)
    eqtime=229.18*(0.000075+0.001868*Math.cos(gamma)-0.032077*Math.sin(gamma)-0.014615*Math.cos(2*gamma)-0.040849*Math.sin(2*gamma))
    decl=0.006918-0.399912*Math.cos(gamma)+0.070257*Math.sin(gamma)-0.006758*Math.cos(2*gamma)+0.000907*Math.sin(2*gamma)-0.002697*Math.cos(3*gamma)+0.00148*Math.sin(3*gamma)
    solar_minutes=(hour*60.0+eqtime+4.0*lon-60.0*tz)%1440.0
    hour_angle=(solar_minutes/4.0-180.0)*Math::PI/180.0
    latr=lat*Math::PI/180.0
    cos_zenith=Math.sin(latr)*Math.sin(decl)+Math.cos(latr)*Math.cos(decl)*Math.cos(hour_angle)
    elevation=90.0-Math.acos([[cos_zenith,-1.0].max,1.0].min)*180.0/Math::PI
    azimuth=(Math.atan2(Math.sin(hour_angle),Math.cos(hour_angle)*Math.sin(latr)-Math.tan(decl)*Math.cos(latr))*180.0/Math::PI+180.0)%360.0
    [azimuth, [[elevation,-5.0].max,89.0].min]
  end
  def self.xyz(p); [p.x,p.y,p.z].map{|v| num(v)}.join(' '); end
  def self.rgb(mat)
    return [0.8,0.8,0.8] unless mat
    c=mat.color; [c.red/255.0,c.green/255.0,c.blue/255.0]
  end
  def self.material_record(mat,index,texture_dir)
    name=mat ? mat.name : 'Default'; col=rgb(mat); alpha=mat ? mat.alpha.to_f : 1.0
    upper=name.upcase
    rough=upper[/RV_ROUGH_([0-9.]+)/,1].to_f if upper.include?('RV_ROUGH_')
    metal=upper[/RV_METAL_([0-9.]+)/,1].to_f if upper.include?('RV_METAL_')
    rough=0.65 if rough.nil?; metal=0.0 if metal.nil?
    rough=rough.clamp(0.045,1.0); metal=metal.clamp(0.0,1.0)
    emission=upper.start_with?('RV_EMIT_') ? col.map{|v|v*12.0} : [0.0,0.0,0.0]
    record="mat #{name.to_json} #{(col+[rough,metal,(1.0-alpha).clamp(0.0,1.0),1.5]+emission).map{|v|num(v)}.join(' ')}\n"
    if mat && mat.texture
      begin
        FileUtils.mkdir_p(texture_dir)
        tex_path=File.join(texture_dir,"material_#{index}.bmp")
        mat.texture.write(tex_path)
        record << "tex #{index} #{tex_path.to_json}\n" if File.file?(tex_path) && File.size(tex_path)>54
      rescue
        # Unsupported texture export is reported as a material-color fallback by the engine.
      end
    end
    record
  end
  def self.uv_pair(uvh, point)
    q=uvh.get_front_UVQ(point)
    return [0.0,0.0] if q.z.abs < 1.0e-9
    [q.x/q.z,q.y/q.z]
  rescue
    [0.0,0.0]
  end
  def self.walk_entities(entities,tr,mats,lines,texture_dir,texture_writer)
    entities.each do |e|
      if e.is_a?(Sketchup::Face)
        mat=e.material || e.back_material;key=mat ? mat.object_id : 0
        unless mats.key?(key);mats[key]=mats.length;lines<<material_record(mat,mats[key],texture_dir);end
        mesh=e.mesh(7);uvh=e.get_UVHelper(true,true,texture_writer)
        mesh.polygons.each do |poly|
          ids=poly.map{|i|i.abs};next if ids.length<3
          local_pts=ids.map{|i|mesh.point_at(i)}
          pts=local_pts.map{|pt|pt.transform(tr)}
          uvs=local_pts.map{|pt|uv_pair(uvh,pt)}
          (1...(pts.length-1)).each{|j|a=uvs[0];b=uvs[j];c=uvs[j+1];lines<<"triuv #{xyz(pts[0])} #{xyz(pts[j])} #{xyz(pts[j+1])} #{[a[0],a[1],b[0],b[1],c[0],c[1]].map{|v|num(v)}.join(' ')} #{mats[key]}\n"}
        end
      elsif e.is_a?(Sketchup::Group)
        walk_entities(e.entities,tr*e.transformation,mats,lines,texture_dir,texture_writer)
      elsif e.is_a?(Sketchup::ComponentInstance)
        walk_entities(e.definition.entities,tr*e.transformation,mats,lines,texture_dir,texture_writer)
      end
    end
  end
  def self.export_scene(path, sun_az=135.0, sun_el=35.0, sun_power=3.0)
    model=Sketchup.active_model;mats={};lines=[];texture_dir=File.join(File.dirname(path),"rv_textures");texture_writer=Sketchup.create_texture_writer
    walk_entities(model.entities,Geom::Transformation.new,mats,lines,texture_dir,texture_writer)
    cam=model.active_view.camera
    lines<<"camera #{xyz(cam.eye)} #{xyz(cam.target)} #{xyz(cam.up)} #{num(cam.fov)}\n"
    lines<<"world 0.18 0.20 0.24\n"
    sh=model.shadow_info
    az=sh['SunDirection'] rescue nil
    lines<<"sun #{num(sun_az)} #{num(sun_el)} #{num(sun_power)} 1\n"
    File.write(path,"# REALVISION RENDER SCENE 1.0\n"+lines.join)
  end
  def self.show
    @dlg ||= UI::HtmlDialog.new(dialog_title:'REALVISION RENDER 1.14',preferences_key:'rv_engine_114',width:470,height:760,resizable:true,scrollable:true)
    @dlg.set_file(File.join(ROOT,'realvision_render','ui','panel.html'))
    @dlg.add_action_callback('choose_engine') do |d,_|
      p=UI.openpanel('Pilih realvision_render.exe',nil,'Executable|*.exe||')
      if p; @engine_path=p; Sketchup.write_default('RealVisionRender','engine_path',p); d.execute_script("setEngine(#{p.to_json})"); end
    end
    @dlg.add_action_callback('choose_hdri') do |d,_|
      p=UI.openpanel('Pilih HDRI Radiance',nil,'HDRI|*.hdr;*.HDR||')
      d.execute_script("setHdri(#{p.to_json})") if p
    end
    @dlg.add_action_callback('choose_ies') do |d,_|
      p=UI.openpanel('Pilih profil IES',nil,'IES Photometric|*.ies;*.IES||')
      d.execute_script("document.getElementById('ies').value=#{p.to_json}") if p
    end
    @dlg.add_action_callback('export_scene') do |d,_|
      begin
        path=UI.savepanel('Simpan scene REALVISION','.','scene.rvscene')
        if path
          path += '.rvscene' unless path.downcase.end_with?('.rvscene')
          export_scene(path);d.execute_script("showStatus(#{("Scene diekspor: "+path).to_json})")
        end
      rescue => e
        d.execute_script("showStatus(#{("Gagal ekspor: "+e.message).to_json})")
      end
    end
    @dlg.add_action_callback('render_preview') do |d,payload|
      begin
        o=JSON.parse(payload);engine=o['engine'].to_s
        engine=Sketchup.read_default('RealVisionRender','engine_path','').to_s if engine.empty?
        raise 'Pilih realvision_render.exe terlebih dahulu.' unless File.file?(engine)
        work=File.join(ENV['TEMP']||Dir.tmpdir,'realvision_render_113');FileUtils.mkdir_p(work)
        scene=File.join(work,'sketchup_scene.rvscene');out=File.join(work,'REALVISION_PREVIEW_113.bmp')
        sun_az=o['sunaz'] || 135; sun_el=o['sunel'] || 35
        if o['solar_enabled'].to_s=='true'
          sun_az,sun_el=solar_position(o['solar_date'],o['solar_time'],o['latitude'],o['longitude'],o['timezone'])
        end
        export_scene(scene,sun_az,sun_el,o['sunpower'] || 3)
        samples=[[o['samples'].to_i,1].max,256].min
        width=[[o['width'].to_i,160].max,4096].min
        height=[[o['height'].to_i,160].max,4096].min
        hdri=o['hdri'].to_s
        ies=o['ies'].to_s
        exposure=[[o['exposure'].to_f,-5.0].max,5.0].min
        bloom=[[o['bloom'].to_f,0.0].max,2.0].min
        # Pass argv as an array: Ruby/Windows handles spaces in paths safely.
        args=['--scene',scene,'--out',out,'--samples',samples.to_s,
              '--width',width.to_s,'--height',height.to_s,
              '--exposure',num(exposure),'--bloom',num(bloom)]
        args += ['--hdri',hdri] if File.file?(hdri)
        if File.file?(ies)
          args += ['--ies',ies,'--ies-x',num(o['iesx']),'--ies-y',num(o['iesy']),
                   '--ies-z',num(o['iesz']),'--ies-power',num(o['iespower'])]
        end
        d.execute_script("showStatus(#{("Render CPU reference; posisi matahari azimuth #{format('%.1f',sun_az)}°, elevasi #{format('%.1f',sun_el)}°...").to_json})")
        ok=system(engine,*args)
        if ok && File.file?(out)
          UI.openURL("file:///#{out.gsub('\\\\','/')}")
          d.execute_script("showStatus(#{("Preview selesai: "+out).to_json})")
        else
          d.execute_script("showStatus('Render gagal. Periksa engine dan file scene.')")
        end
      rescue => e
        d.execute_script("showStatus(#{("Render gagal: "+e.message).to_json})")
      end
    end
    @dlg.add_action_callback('gpu_check') do |d,engine|
      begin
        engine=engine.to_s
        engine=Sketchup.read_default('RealVisionRender','engine_path','').to_s if engine.empty?
        raise 'Pilih realvision_render.exe terlebih dahulu.' unless File.file?(engine)
        output=`"#{engine}" --gpu-info 2>&1`
        rt=`"#{engine}" --rt-init 2>&1`
        report="Diagnostik Vulkan:\n#{output}\nInisialisasi RT:\n#{rt}"
        d.execute_script("showStatus(#{report.to_json})")
      rescue => e
        d.execute_script("showStatus(#{('Diagnostik gagal: '+e.message).to_json})")
      end
    end
    @dlg.add_action_callback('ping') do |d,_|
      begin
        s=TCPSocket.new('127.0.0.1',47831);s.write("PING\n");res=s.gets.to_s.strip;s.close
        d.execute_script("showStatus(#{res.to_json})")
      rescue
        d.execute_script("showStatus('Bridge service belum berjalan; render preview dapat dijalankan langsung dari executable.')")
      end
    end
    @dlg.show
  end
  unless file_loaded?(__FILE__)
    UI.menu('Extensions').add_item('REALVISION RENDER 1.14'){show}
    file_loaded(__FILE__)
  end
end
